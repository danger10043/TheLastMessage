#include "Character/Enemy/EnemyProjectile.h"
#include "Character/Enemy/EnemyBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 5.f; // 생성 측에서 FEnemyProjectileConfig::Lifetime으로 SetLifeSpan 가능

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

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 1500.f;
	ProjectileMovement->MaxSpeed = 1500.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bAutoActivate = false; // LaunchProjectile에서 활성화
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

	ProjectileMovement->Velocity = Dir * ProjectileMovement->InitialSpeed;
	ProjectileMovement->Activate(true);
}

void AEnemyProjectile::HandleProjectileImpact(AActor* HitActor)
{
	if (bImpactHandled || !HitActor || HitActor == this || HitActor == SourceEnemy)
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
	Destroy();
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
