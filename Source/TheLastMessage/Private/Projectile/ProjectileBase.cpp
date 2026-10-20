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
#include "DataAsset/WeaponDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

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
	ProjectileMesh->SetRelativeScale3D(FVector(0.45f, 0.12f, 0.12f));
	ProjectileMesh->SetCastShadow(false);
	FlightEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlightEffect"));
	FlightEffect->SetupAttachment(RootComponent);

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
	OnRep_VisualData();
	if (!bUseWeaponVisualSettings)
	{
		if (UMaterialInstanceDynamic* Material = ProjectileMesh->CreateDynamicMaterialInstance(0))
			Material->SetVectorParameterValue(TEXT("ProjectileColor"), ProjectileTint);
	}
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

void AProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectileBase, VisualData);
}

void AProjectileBase::ConfigureVisuals(UWeaponDataAsset* Data)
{
	if (!HasAuthority()) return;
	VisualData = Data;
	OnRep_VisualData();
}

void AProjectileBase::OnRep_VisualData()
{
	if (!bUseWeaponVisualSettings || !VisualData || !ProjectileMesh) return;
	const FVector Scale = VisualData->ProjectileVisualScale;
	if (!Scale.ContainsNaN() && Scale.GetMin() > 0.0f) ProjectileMesh->SetRelativeScale3D(Scale);
	if (VisualData->ProjectileMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(VisualData->ProjectileMaterial, this);
		Material->SetVectorParameterValue(TEXT("ProjectileColor"), VisualData->ProjectileColor);
		ProjectileMesh->SetMaterial(0, Material);
	}
}

bool AProjectileBase::InitializeProjectile(AActor* Source, TSubclassOf<UGameplayEffect> Effect, float InDamage, float Speed, float Range, float InFeedbackStrength)
{
	if (!HasAuthority() || !IsValid(Source) || !Effect || !FMath::IsFinite(InDamage) || InDamage <= 0.0f
		|| !FMath::IsFinite(Speed) || Speed <= 0.0f || !FMath::IsFinite(Range) || Range <= 0.0f) return false;
	SourceActor = Source;
	DamageEffectClass = Effect;
	Damage = InDamage;
	FeedbackStrength = InFeedbackStrength;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	const float EffectiveRange = bOverrideWeaponRange ? TravelRange : Range;
	if (!FMath::IsFinite(EffectiveRange) || EffectiveRange <= 0.0f) return false;
	MaxTravelTime = EffectiveRange / Speed;
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
	MulticastImpact(GetActorLocation());
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();
	if ((Enemy && !Enemy->IsDead()) || (Boss && Boss->GetBossState() != EBossState::Dead))
	{
		UCombatDamageLibrary::ApplyFinalDamageToTarget(SourceActor.Get(), OtherActor, DamageEffectClass,
			Damage, WeaponGameplayTags::Damage_PlayerRanged, FeedbackStrength);
	}
	DestroyProjectile();
}

void AProjectileBase::DestroyProjectile()
{
	Destroy();
}

void AProjectileBase::MulticastImpact_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (ImpactEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, Location, GetActorRotation());
	if (ImpactSound) UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Location);
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
