#include "Character/Enemy/EnemyAimLaserComponent.h"
#include "Character/Enemy/EnemyBase.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UEnemyAimLaserComponent::UEnemyAimLaserComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 몬스터를 따라 움직이지 않고 월드 좌표로 직접 배치한다.
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	bReceivesDecals = false;
	SetVisibility(false);
}

void UEnemyAimLaserComponent::Configure(float InWidth, float InMaxLength)
{
	Width = FMath::Max(InWidth, 0.1f);
	MaxLength = FMath::Max(InMaxLength, 1.f);
}

void UEnemyAimLaserComponent::Show(AEnemyBase* InOwnerEnemy, float InLockDelay)
{
	if (!InOwnerEnemy)
	{
		return;
	}

	OwnerEnemy = InOwnerEnemy;
	LockDelay = FMath::Max(InLockDelay, 0.f);
	Elapsed = 0.f;
	bLocked = false;
	CurrentDirection = FVector::ZeroVector;
	LockedDirection = FVector::ZeroVector;
	bShown = true;

	SetComponentTickEnabled(true);
	UpdateLaser();
}

void UEnemyAimLaserComponent::Hide()
{
	bShown = false;
	bLocked = false;
	LockedDirection = FVector::ZeroVector;
	SetVisibility(false);
	SetComponentTickEnabled(false);
}

void UEnemyAimLaserComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShown)
	{
		return;
	}

	Elapsed += DeltaTime;
	UpdateLaser();
}

void UEnemyAimLaserComponent::UpdateLaser()
{
	AEnemyBase* Enemy = OwnerEnemy.Get();
	UWorld* World = GetWorld();
	if (!Enemy || !World || !bShown)
	{
		SetVisibility(false);
		return;
	}

	// 투사체는 몬스터가 있는 Y 평면에서 나간다. 레이저도 같은 평면에 그린다.
	FVector Muzzle = Enemy->GetProjectileMuzzleLocation();
	Muzzle.Y = Enemy->GetActorLocation().Y;

	if (!bLocked)
	{
		const FVector Aim = Enemy->ComputeAimDirection(Muzzle);
		if (!Aim.IsNearlyZero())
		{
			CurrentDirection = Aim;
		}

		// 정해진 시간이 지나면 지금 방향으로 고정한다. 이후 발사되는 탄도 이 방향으로 나간다.
		if (Elapsed >= LockDelay && !CurrentDirection.IsNearlyZero())
		{
			bLocked = true;
			LockedDirection = CurrentDirection;
		}
	}

	const FVector Direction = bLocked ? LockedDirection : CurrentDirection;
	FVector Flat(Direction.X, 0.f, Direction.Z);
	if (Flat.IsNearlyZero())
	{
		SetVisibility(false);
		return;
	}
	Flat.Normalize();

	// 벽에 막히면 벽까지만 그린다. 투사체는 월드 정적 물체에 막힌다.
	float Length = MaxLength;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAimLaser), false, Enemy);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Muzzle, Muzzle + Flat * MaxLength, ObjectParams, QueryParams))
	{
		Length = FMath::Max(Hit.Distance, 1.f);
	}

	// 평면 메시(가로세로 100)를 세워서 옆면 카메라가 볼 수 있게 한다. X가 길이, Y가 굵기다.
	FRotator Rotation = Flat.Rotation();
	Rotation.Roll = 90.f;

	SetWorldRotation(Rotation);
	SetWorldLocation(Muzzle + Flat * (Length * 0.5f));
	SetWorldScale3D(FVector(Length / 100.f, Width / 100.f, 1.f));
	SetVisibility(true);
}
