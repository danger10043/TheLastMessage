#include "Character/Enemy/SentryEnemy.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

ASentryEnemy::ASentryEnemy()
{
	EnemyType = EEnemyType::Sentry;

	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

	// 기본값(임시). 블루프린트·데이터 에셋에서 조정한다.
	StatConfig.MaxHealth = 150.f;
	StatConfig.MoveSpeed = 0.f;
	StatConfig.DetectRange = 1800.f;
	StatConfig.AttackRange = 1500.f;
	StatConfig.AttackCooldown = 3.f;

	ProjectileConfig.Damage = 40.f;
	ProjectileConfig.Speed = 700.f;
	ProjectileConfig.Lifetime = 6.f;
}

void ASentryEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 어디든 고정 설치할 수 있도록 이동과 중력을 끈다.
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	GetCharacterMovement()->GravityScale = 0.f;
}

void ASentryEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RotateToTarget(DeltaTime);
}

void ASentryEnemy::RotateToTarget(float DeltaTime)
{
	const AActor* Target = GetCurrentTarget();
	if (!Target || IsDead())
	{
		return;
	}

	// 조준 중심: 총 상하 회전축 본이 있으면 그 위치, 없으면 액터 위치
	FVector Origin = GetActorLocation();
	if (GetMesh() && GetMesh()->DoesSocketExist(AimPivotBoneName))
	{
		Origin = GetMesh()->GetSocketLocation(AimPivotBoneName);
	}

	const FVector ToTargetWorld = Target->GetActorLocation() - Origin;
	if (ToTargetWorld.IsNearlyZero())
	{
		return;
	}

	// 몸체 기준 방향으로 바꿔 좌우·상하 각도를 구한다. 액터 자체는 회전하지 않는다.
	const FVector ToTargetLocal = GetActorTransform().InverseTransformVectorNoScale(ToTargetWorld);
	const FRotator Desired = ToTargetLocal.Rotation();

	const float DesiredYaw = FRotator::NormalizeAxis(Desired.Yaw + AimYawOffset);
	const float DesiredPitch = FMath::Clamp(Desired.Pitch, MinAimPitch, MaxAimPitch);

	const float MaxStep = RotationSpeed * DeltaTime;
	const float YawDelta = FMath::Clamp(FRotator::NormalizeAxis(DesiredYaw - AimYaw), -MaxStep, MaxStep);
	AimYaw = FRotator::NormalizeAxis(AimYaw + YawDelta);
	AimPitch = FMath::Clamp(AimPitch + FMath::Clamp(DesiredPitch - AimPitch, -MaxStep, MaxStep), MinAimPitch, MaxAimPitch);
}

void ASentryEnemy::StartAttack()
{
	if (IsDead() || IsAttacking() || !GetCurrentTarget())
	{
		return;
	}

	StartCharge();
}

void ASentryEnemy::StartCharge()
{
	bIsAttacking = true;
	bIsCharging = true;
	OnChargeStarted();

	GetWorldTimerManager().SetTimer(ChargeTimerHandle, this, &ASentryEnemy::FireProjectile, ChargeTime, false);
}

void ASentryEnemy::FireProjectile()
{
	// 충전 중 타겟이 사라지거나 몬스터가 죽었다면 발사하지 않고 취소한다.
	if (IsDead() || !GetCurrentTarget())
	{
		CancelAttack();
		return;
	}

	bIsCharging = false;

	// 총구 본·소켓 위치에서 발사한다. 없으면 로컬 오프셋을 쓴다.
	FVector Muzzle = GetActorTransform().TransformPosition(MuzzleOffset);
	if (GetMesh() && GetMesh()->DoesSocketExist(MuzzleSocketName))
	{
		Muzzle = GetMesh()->GetSocketLocation(MuzzleSocketName);
	}

	SpawnEnemyProjectile(ProjectileConfig, Muzzle);
	OnFired();

	FinishAttack();
}

void ASentryEnemy::CancelCharge()
{
	CancelAttack();
}

void ASentryEnemy::CancelAttack()
{
	GetWorldTimerManager().ClearTimer(ChargeTimerHandle);

	if (bIsCharging)
	{
		bIsCharging = false;
		OnChargeCancelled();
	}

	Super::CancelAttack();
}
