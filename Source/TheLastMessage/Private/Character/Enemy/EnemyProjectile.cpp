#include "Character/Enemy/EnemyProjectile.h"
#include "Character/Enemy/EnemyBase.h"
#include "Framework/ActorPoolSubsystem.h"
#include "TimerManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 0.f; // 수명은 SetProjectileLifetime(타이머)으로 관리하고 풀로 돌려보낸다.

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(15.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionComponent);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FlareEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlareEffect"));
	FlareEffect->SetupAttachment(CollisionComponent);
	FlareEffect->bAutoActivate = true;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 1500.f;
	ProjectileMovement->MaxSpeed = 1500.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bAutoActivate = false; // LaunchProjectile에서 활성화

	// 사이드뷰라 투사체는 발사된 Y 평면을 벗어나지 않는다.
	ProjectileMovement->bConstrainToPlane = true;
	ProjectileMovement->SetPlaneConstraintNormal(FVector(0.f, 1.f, 0.f));
}

void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &AEnemyProjectile::OnCollisionBeginOverlap);
	CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &AEnemyProjectile::OnCollisionHit);
}

void AEnemyProjectile::InitializeProjectile(AEnemyBase* InSource, float InDamage)
{
	SourceEnemy = InSource;
	Damage = InDamage;

	if (SourceEnemy)
	{
		CollisionComponent->IgnoreActorWhenMoving(SourceEnemy, true);
	}
}

void AEnemyProjectile::SetProjectileLifetime(float Lifetime)
{
	if (Lifetime > 0.f)
	{
		GetWorldTimerManager().SetTimer(LifetimeTimerHandle, this, &AEnemyProjectile::ReturnToPool, Lifetime, false);
	}
}

void AEnemyProjectile::SetProjectileSpeed(float InSpeed)
{
	if (InSpeed > 0.f)
	{
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
	}
}

void AEnemyProjectile::LaunchProjectile(FVector Direction)
{
	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		DestroyProjectile();
		return;
	}

	// 벽 등에 막혀 멈춘 투사체는 이동 컴포넌트가 이동 대상(UpdatedComponent)을 놓아 버린다.
	// 풀에서 재사용할 때 다시 연결하지 않으면 움직이지 않고 그 자리에 머문다.
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->SetPlaneConstraintOrigin(GetActorLocation());
	ProjectileMovement->Velocity = Dir * ProjectileMovement->InitialSpeed;
	ProjectileMovement->Activate(true);
	bLaunched = true;

	// 풀에서 위치를 옮긴 직후라 겹침 정보를 최신으로 갱신한다.
	CollisionComponent->UpdateOverlaps();

	// 스폰되는 순간 이미 대상과 겹쳐 있던 경우(바로 앞에 붙은 경우)는 스폰 중의 겹침 이벤트가 무시되므로,
	// 발사자·피해량 설정이 끝난 이 시점에 겹친 대상을 한 번 처리한다.
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		HandleProjectileImpact(OverlappingActor);
	}
}

void AEnemyProjectile::HandleProjectileImpact(AActor* HitActor)
{
	// 발사(LaunchProjectile) 전에는 발사자·피해량이 아직 설정되지 않았으므로 충돌을 처리하지 않는다.
	if (!bLaunched || bImpactHandled || !HitActor || HitActor == this || HitActor == SourceEnemy)
	{
		return;
	}

	// 다른 몬스터와 다른 투사체는 그냥 통과한다.
	if (HitActor->IsA<AEnemyBase>() || HitActor->IsA<AEnemyProjectile>())
	{
		return;
	}

	bImpactHandled = true;

	// 플레이어 폰에만 피해를 준다.
	const APawn* HitPawn = Cast<APawn>(HitActor);
	if (HitPawn && HitPawn->IsPlayerControlled())
	{
		if (SourceEnemy)
		{
			SourceEnemy->ApplyEnemyDamage(HitActor, Damage);
		}
		else
		{
			// 발사한 몬스터가 이미 사라졌다면 엔진 기본 피해로 대신한다.
			UGameplayStatics::ApplyDamage(HitActor, Damage, nullptr, this, nullptr);
		}
	}

	DestroyProjectile();
}

void AEnemyProjectile::DestroyProjectile()
{
	if (IsActorBeingDestroyed() || bInPool)
	{
		return;
	}

	if (ImpactEffect && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactEffect, GetActorLocation(), GetActorRotation());
	}

	ReturnToPool();
}

void AEnemyProjectile::ReturnToPool()
{
	if (bInPool || IsActorBeingDestroyed())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UActorPoolSubsystem* Pool = World->GetSubsystem<UActorPoolSubsystem>())
		{
			Pool->ReleaseActor(this);
			return;
		}
	}

	Destroy();
}

void AEnemyProjectile::OnAcquiredFromPool()
{
	bInPool = false;
	bLaunched = false;
	bImpactHandled = false;
	SourceEnemy = nullptr;
	Damage = 0.f;

	CollisionComponent->ClearMoveIgnoreActors();

	if (FlareEffect)
	{
		FlareEffect->Activate(true);
	}
}

void AEnemyProjectile::OnReturnedToPool()
{
	bInPool = true;
	bLaunched = false;
	bImpactHandled = false;
	SourceEnemy = nullptr;
	Damage = 0.f;

	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	CollisionComponent->ClearMoveIgnoreActors();

	if (FlareEffect)
	{
		FlareEffect->DeactivateImmediate();
	}
}

void AEnemyProjectile::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandleProjectileImpact(OtherActor);
}

void AEnemyProjectile::OnCollisionHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// 벽·바닥 등 막는 지오메트리에 닿으면 제거
	HandleProjectileImpact(OtherActor);
	if (!bImpactHandled)
	{
		DestroyProjectile();
	}
}
