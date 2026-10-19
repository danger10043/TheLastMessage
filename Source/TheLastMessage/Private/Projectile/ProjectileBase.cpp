#include "Projectile/ProjectileBase.h"
#include "GameplayEffect.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Boss/BossMonster.h"
#include "GAS/WeaponGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

AProjectileBase::AProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;

	InitialLifeSpan = 5.f;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->SetSphereRadius(5.f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(true);
	RootComponent = CollisionComponent;

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ProjectileMesh->SetStaticMesh(Sphere.Object);
	ProjectileMesh->SetRelativeScale3D(FVector(0.1f));

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 1500.f;
	ProjectileMovement->MaxSpeed = 1500.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bAutoActivate = false;
}

void AProjectileBase::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProjectileMovement->Deactivate();
		return;
	}

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &AProjectileBase::OnCollisionBeginOverlap);
		CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &AProjectileBase::OnCollisionHit);
	}
}

bool AProjectileBase::InitializeProjectile(AActor* Source, TSubclassOf<UGameplayEffect> Effect, float InDamage, float Speed, float Range)
{
	if (!HasAuthority() || !IsValid(Source) || !Effect || !FMath::IsFinite(InDamage) || InDamage <= 0.0f
		|| !FMath::IsFinite(Speed) || Speed <= 0.0f || !FMath::IsFinite(Range) || Range <= 0.0f) return false;
	SourceActor = Source;
	DamageEffectClass = Effect;
	Damage = InDamage;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	MaxTravelTime = Range / Speed;
	CollisionComponent->IgnoreActorWhenMoving(Source, true);
	return true;
}

void AProjectileBase::LaunchProjectile(const FVector& Direction)
{
	if (!HasAuthority() || bLaunched || !SourceActor.IsValid() || Direction.ContainsNaN()
		|| Direction.IsNearlyZero() || MaxTravelTime <= 0.0f)
	{
		return;
	}
	bLaunched = true;
	SetLifeSpan(MaxTravelTime);
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Direction.GetSafeNormal() * ProjectileMovement->InitialSpeed;
		ProjectileMovement->Activate(true);
	}
	TArray<AActor*> Overlaps;
	GetOverlappingActors(Overlaps);
	for (AActor* Actor : Overlaps) HandleImpact(Actor, false);
}

void AProjectileBase::HandleImpact(AActor* OtherActor, bool bBlocking)
{
	if (!HasAuthority() || !bLaunched || bImpactHandled || !IsValid(OtherActor)
		|| OtherActor == this || OtherActor == GetOwner() || OtherActor == SourceActor.Get()) return;
	const AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	ABossMonster* Boss = Cast<ABossMonster>(OtherActor);
	if (!Enemy && !Boss && !bBlocking) return;
	bImpactHandled = true;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();
	if ((Enemy && !Enemy->IsDead()) || (Boss && Boss->GetBossState() != EBossState::Dead))
	{
		UCombatDamageLibrary::ApplyFinalDamageToTarget(SourceActor.Get(), OtherActor, DamageEffectClass,
			Damage, WeaponGameplayTags::Damage_PlayerRanged);
	}
	DestroyProjectile();
}

void AProjectileBase::DestroyProjectile()
{
	Destroy();
}

void AProjectileBase::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandleImpact(OtherActor, false);
}

void AProjectileBase::OnCollisionHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	HandleImpact(OtherActor, true);
}
