#include "Character/Boss/BossLightning.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "CollisionQueryParams.h"
#include "Framework/ActorPoolSubsystem.h"
#include "TimerManager.h"

ABossLightning::ABossLightning()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* SceneRoot =
        CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    WarningCylinder =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningCylinder"));
    WarningCylinder->SetupAttachment(SceneRoot);
    WarningCylinder->SetAbsolute(true, true, true);
    WarningCylinder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WarningCylinder->SetGenerateOverlapEvents(false);
    WarningCylinder->SetCastShadow(false);
    WarningCylinder->SetCanEverAffectNavigation(false);
}

void ABossLightning::BeginPlay()
{
    Super::BeginPlay();

    SetLifeSpan(0.f);
    bStruck = true;
    SetActorTickEnabled(false);
    WarningCylinder->SetVisibility(false);
}

bool ABossLightning::StartLightning(
    float InDamage, float InRadius, float InWarningTime)
{
    if (bInPool || IsActorBeingDestroyed() || !GetWorld())
    {
        return false;
    }

    Damage = InDamage;
    Radius = InRadius;
    WarningTime = InWarningTime;
    SourceBoss = Cast<ABossMonster>(GetOwner());                         
    const UStaticMesh* Mesh = WarningCylinder->GetStaticMesh();
    const UGameplayEffect* EffectDefaults = DamageEffectClass
        ? DamageEffectClass.GetDefaultObject() : nullptr;

    if (!IsValid(SourceBoss.Get()) || !IsValid(Mesh)
        || !IsValid(LightningEffect.Get()) || !EffectDefaults
        || EffectDefaults->DurationPolicy != EGameplayEffectDurationType::Instant
        || !FMath::IsFinite(Damage) || Damage < 0.f
        || !FMath::IsFinite(Radius) || Radius < 1.f
        || !FMath::IsFinite(WarningTime) || WarningTime < 0.f)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: invalid lightning settings or Owner."), *GetName());
        ReturnToPool();
        return false;
    }

    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const double MeshRadius = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
    const double HeightScale =
        FMath::Abs(WarningCylinder->GetComponentScale().Z);
    if (!FMath::IsFinite(MeshRadius) || MeshRadius <= 0.0
        || !FMath::IsFinite(Bounds.BoxExtent.Z) || Bounds.BoxExtent.Z <= 0.0
        || !FMath::IsFinite(HeightScale) || HeightScale <= 0.0
        || Bounds.Origin.ContainsNaN())
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: invalid warning cylinder mesh bounds or scale."), *GetName());
        ReturnToPool();
        return false;
    }

    const FVector TraceStart = GetActorLocation();
    const FVector TraceEnd = TraceStart - FVector(0.f, 0.f, 10000.f);

    FCollisionObjectQueryParams ObjectQuery;
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(BossLightningGroundTrace), false, this);
    QueryParams.AddIgnoredActor(SourceBoss.Get());

    FHitResult GroundHit;
    if (GetWorld()->LineTraceSingleByObjectType(
        GroundHit, TraceStart, TraceEnd, ObjectQuery, QueryParams))
    {
        SetActorLocation(
            GroundHit.ImpactPoint, false, nullptr,
            ETeleportType::TeleportPhysics);
    }

    StrikeLocation = GetActorLocation();
    CylinderHalfHeight = Bounds.BoxExtent.Z * HeightScale;
    const FVector WarningScale(Radius / MeshRadius, Radius / MeshRadius, HeightScale);
    const FVector WarningCenter = StrikeLocation + FVector(0.f, 0.f, 800.f);

    // Mesh bounds 중심을 액터보다 800cm 위에 배치한다.
    WarningCylinder->SetWorldRotation(FRotator::ZeroRotator);
    WarningCylinder->SetWorldScale3D(WarningScale);
    WarningCylinder->SetWorldLocation(
        WarningCenter - Bounds.Origin * WarningScale);
    WarningCylinder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WarningCylinder->SetGenerateOverlapEvents(false);

    WarningMID = WarningCylinder->CreateDynamicMaterialInstance(0);
    if (!IsValid(WarningMID.Get()))
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: failed to create warning material."), *GetName());
        ReturnToPool();
        return false;
    }

    WarningMID->SetScalarParameterValue(TEXT("Progress"), 0.f);
    WarningStartTime = GetWorld()->GetTimeSeconds();
    bStruck = false;
    WarningCylinder->SetVisibility(true);
    SetActorTickEnabled(true);

    if (WarningTime == 0.f)
    {
        Strike();
    }
    return true;
}

void ABossLightning::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bStruck)
    {
        return;
    }

    const double Elapsed = GetWorld()->GetTimeSeconds() - WarningStartTime;
    const float Progress = WarningTime > 0.f
        ? FMath::Clamp(static_cast<float>(Elapsed / WarningTime), 0.f, 1.f)
        : 1.f;
    if (IsValid(WarningMID.Get()))
    {
        WarningMID->SetScalarParameterValue(TEXT("Progress"), Progress);
    }

    if (Elapsed >= WarningTime)
    {
        Strike();
    }
}

void ABossLightning::Strike()
{
    if (bStruck)
    {
        return;
    }
    bStruck = true;
    SetActorTickEnabled(false);
    WarningCylinder->SetVisibility(false);

    if (GetNetMode() != NM_DedicatedServer)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, LightningEffect.Get(),
            StrikeLocation - FVector(0.f, 0.f, 50.f));
    }

    if (HasAuthority())
    {
        ApplyStrikeDamage();
    }

    ReturnToPool();
}

bool ABossLightning::IsPlayerInsideCylinder(const APlayerBase* Player) const
{
    const UCapsuleComponent* Capsule = Player ? Player->GetCapsuleComponent() : nullptr;
    if (!IsValid(Capsule))
    {
        return false;
    }

    float CapsuleRadius = 0.f;
    float CapsuleHalfHeight = 0.f;
    Capsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
    const FVector Center = Capsule->GetComponentLocation();
    const float SegmentHalfHeight =
        FMath::Max(0.f, CapsuleHalfHeight - CapsuleRadius);

    const FVector WarningCenter = StrikeLocation + FVector(0.f, 0.f, 800.f);

    const double RadialGap = FMath::Max(
        0.0, FVector::Dist2D(Center, WarningCenter) - Radius);
    const double VerticalGap = FMath::Max(
        0.0, FMath::Abs(Center.Z - WarningCenter.Z)
        - CylinderHalfHeight - SegmentHalfHeight);

    return FMath::Square(RadialGap) + FMath::Square(VerticalGap)
        <= FMath::Square(static_cast<double>(CapsuleRadius));
}

void ABossLightning::ApplyStrikeDamage()
{
    if (!IsValid(SourceBoss.Get()) || Damage <= 0.f)
    {
        return;
    }

    UAbilitySystemComponent* SourceASC = SourceBoss->GetAbilitySystemComponent();
    if (!IsValid(SourceASC))
    {
        return;
    }

    TArray<TWeakObjectPtr<APlayerBase>> HitPlayers;
    for (TActorIterator<APlayerBase> It(GetWorld()); It; ++It)
    {
        APlayerBase* Player = *It;
        if (IsValid(Player) && Player->IsPlayerControlled()
            && !Player->IsDead() && IsPlayerInsideCylinder(Player))
        {
            HitPlayers.Add(Player);
        }
    }

    for (const TWeakObjectPtr<APlayerBase>& Entry : HitPlayers)
    {
        APlayerBase* Player = Entry.Get();
        if (!IsValid(Player) || Player->IsDead())
        {
            continue;
        }

        UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent();
        if (!IsValid(TargetASC)
            || !TargetASC->HasAttributeSetForAttribute(
                UCombatAttributeSet::GetIncomingDamageAttribute()))
        {
            continue;
        }

        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddSourceObject(SourceBoss.Get());
        Context.AddOrigin(StrikeLocation);

        FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
            DamageEffectClass, 1.f, Context);
        if (!Spec.IsValid())
        {
            continue;
        }

        Spec.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_Damage, Damage);
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    }
}

void ABossLightning::OnAcquiredFromPool()
{
    bInPool = false;
    bStruck = true;
    SourceBoss = nullptr;
    StrikeLocation = FVector::ZeroVector;
    WarningStartTime = 0.0;
    CylinderHalfHeight = 0.f;
    SetLifeSpan(0.f);
    SetActorTickEnabled(false);
    SetActorEnableCollision(false);
    WarningCylinder->SetVisibility(false);
}

void ABossLightning::OnReturnedToPool()
{
    bInPool = true;
    bStruck = true;
    SourceBoss = nullptr;
    SetLifeSpan(0.f);
    GetWorldTimerManager().ClearAllTimersForObject(this);
    SetActorTickEnabled(false);
    WarningCylinder->SetVisibility(false);
}

void ABossLightning::ReturnToPool()
{
    if (bInPool || IsActorBeingDestroyed())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (UActorPoolSubsystem* Pool =
            World->GetSubsystem<UActorPoolSubsystem>())
        {
            Pool->ReleaseActor(this);
            return;
        }
    }

    OnReturnedToPool();
    Destroy();
}