#include "Character/Boss/BossBullet.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Framework/ActorPoolSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABossBullet::ABossBullet()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(10.f);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->SetCollisionObjectType(ECC_GameTraceChannel5);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Ignore);
    Collision->SetGenerateOverlapEvents(true);
    SetRootComponent(Collision);

    BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh"));
    BulletMesh->SetupAttachment(Collision);
    BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BulletMesh->SetGenerateOverlapEvents(false);

    TrailComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailComponent"));
    TrailComponent->SetupAttachment(Collision);
    TrailComponent->SetAutoActivate(false);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
        TEXT("ProjectileMovement"));
    ProjectileMovement->SetUpdatedComponent(Collision);
    ProjectileMovement->ProjectileGravityScale = 0.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->bSweepCollision = true;
    ProjectileMovement->bAutoActivate = false;

    Collision->OnComponentBeginOverlap.AddDynamic(this, &ABossBullet::OnBeginOverlap);
    ProjectileMovement->OnProjectileStop.AddDynamic(this, &ABossBullet::OnProjectileStop);
}

void ABossBullet::BeginPlay()
{
    Super::BeginPlay();

    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProjectileMovement->Deactivate();
    OnRep_FlightState();
}

void ABossBullet::OnProjectileStop(const FHitResult& Hit)
{
    HandleImpact(Hit.GetActor(), Hit);
}

void ABossBullet::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (Cast<APlayerBase>(OtherActor))
    {
        HandleImpact(OtherActor, bFromSweep ? SweepResult : FHitResult());
    }
}

void ABossBullet::HandleImpact(AActor* OtherActor, const FHitResult& Hit)
{
    if (!HasAuthority() || !bReady || bImpactHandled
        || OtherActor == this
        || (OtherActor && (OtherActor == GetOwner() || OtherActor == GetInstigator())))
    {
        return;
    }

    bImpactHandled = true;
    const FVector ShotDirection = ProjectileMovement->Velocity.IsNearlyZero()
        ? GetActorForwardVector() : ProjectileMovement->Velocity.GetSafeNormal();
    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (APlayerBase* Player = Cast<APlayerBase>(OtherActor))
    {
        if (!Player->IsDead() && Damage > 0.f)
        {
            UGameplayStatics::ApplyPointDamage(Player, Damage, ShotDirection,
                Hit, GetInstigatorController(), this, nullptr);
        }
    }

    const FVector StrikeLocation = Hit.bBlockingHit
        ? FVector(Hit.ImpactPoint) : GetActorLocation();
    const FRotator StrikeRotation = Hit.bBlockingHit
        ? Hit.ImpactNormal.Rotation() : GetActorRotation();
    MulticastStrike(StrikeLocation, StrikeRotation);
    ReturnToPool();
}

void ABossBullet::MulticastStrike_Implementation(FVector Location, FRotator Rotation)
{
    if (GetNetMode() != NM_DedicatedServer && BulletStrike)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, BulletStrike.Get(), Location, Rotation);
    }
}


void ABossBullet::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABossBullet, bFlightActive);
    DOREPLIFETIME(ABossBullet, FlightSerial);
}

void ABossBullet::OnRep_FlightState()
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    if (bFlightActive && BulletTrail)
    {
        TrailComponent->SetAsset(BulletTrail.Get());
        TrailComponent->Activate(true);
    }
    else
    {
        TrailComponent->DeactivateImmediate();
    }
}

void ABossBullet::OnAcquiredFromPool()
{
    bInPool = false;
    bReady = false;
    bImpactHandled = false;
    SetLifeSpan(0.f);
    GetWorldTimerManager().ClearTimer(FlightTimer);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->ClearMoveIgnoreActors();
    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();
    // StopSimulating clears UpdatedComponent; restore it for every reuse.
    ProjectileMovement->SetUpdatedComponent(Collision);
}

bool ABossBullet::StartBullet(float InDamage, float InSpeed, float Lifetime)
{
    if (!HasAuthority() || bInPool || bReady || IsActorBeingDestroyed())
    {
        return false;
    }
    if (!FMath::IsFinite(InDamage) || InDamage < 0.f
        || !FMath::IsFinite(InSpeed) || InSpeed <= 0.f
        || !FMath::IsFinite(Lifetime) || Lifetime <= 0.f)
    {
        ReturnToPool();
        return false;
    }

    Damage = InDamage;
    Speed = InSpeed;
    if (AActor* Source = GetOwner())
    {
        Collision->IgnoreActorWhenMoving(Source, true);
    }
    if (APawn* SourcePawn = GetInstigator())
    {
        Collision->IgnoreActorWhenMoving(SourcePawn, true);
    }

    ProjectileMovement->SetUpdatedComponent(Collision);
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->Velocity = GetActorForwardVector() * Speed;
    bReady = true;
    bFlightActive = true;
    ++FlightSerial;
    OnRep_FlightState();
    ForceNetUpdate();
    GetWorldTimerManager().SetTimer(
        FlightTimer, this, &ABossBullet::ReturnToPool, Lifetime, false);

    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    if (!bReady)
    {
        return true; // Already hit a player during collision activation.
    }
    ProjectileMovement->Activate(true);
    Collision->UpdateOverlaps();
    TArray<AActor*> OverlappingActors;
    Collision->GetOverlappingActors(OverlappingActors, APlayerBase::StaticClass());
    for (AActor* OtherActor : OverlappingActors)
    {
        HandleImpact(OtherActor, FHitResult());
        if (!bReady)
        {
            break;
        }
    }
    return true;
}

void ABossBullet::OnReturnedToPool()
{
    bInPool = true;
    bReady = false;
    bImpactHandled = true;
    bFlightActive = false;
    GetWorldTimerManager().ClearTimer(FlightTimer);
    SetLifeSpan(0.f);
    ProjectileMovement->StopMovementImmediately();
    ProjectileMovement->Deactivate();
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->ClearMoveIgnoreActors();
    OnRep_FlightState();
    ForceNetUpdate();
}

void ABossBullet::ReturnToPool()
{
    if (!HasAuthority() || bInPool || IsActorBeingDestroyed())
    {
        return;
    }
    if (UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>())
    {
        Pool->ReleaseActor(this);
    }
}