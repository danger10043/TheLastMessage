#include "Character/Boss/BossCircularMissile.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossActivationCore.h"
#include "Character/Player/PlayerBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Curves/CurveFloat.h"

ABossCircularMissile::ABossCircularMissile()
{
    PrimaryActorTick.bCanEverTick = true;

    MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));
    SetRootComponent(MissileMesh);
    MissileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MissileMesh->SetGenerateOverlapEvents(false);

    TrailVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailVFX"));
    TrailVFX->SetupAttachment(MissileMesh);

    WarningSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningSphere"));
    WarningSphere->SetupAttachment(MissileMesh);
    WarningSphere->SetAbsolute(true, true, true);
    WarningSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WarningSphere->SetGenerateOverlapEvents(false);
    WarningSphere->SetCastShadow(false);
    WarningSphere->SetCanEverAffectNavigation(false);
}

void ABossCircularMissile::BeginPlay()
{
    Super::BeginPlay();

    SourceBoss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(SourceBoss.Get())
        || !FMath::IsFinite(LaunchTime) || LaunchTime < 0.01f
        || !FMath::IsFinite(Damage) || Damage < 0.f
        || !FMath::IsFinite(DamageRadius) || DamageRadius < 1.f
        || !FMath::IsFinite(MissileLaunchDelta) || MissileLaunchDelta < 0.f
        || FireLocation.ContainsNaN() || !DamageEffectClass
        || DamageEffectClass.GetDefaultObject()->DurationPolicy
        != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: invalid missile settings or Owner."), *GetName());
        Destroy();
        return;
    }

    StartLocation = GetActorLocation();
    MidLocation = StartLocation + FVector(
        FMath::FRandRange(-MissileLaunchDelta, MissileLaunchDelta),
        FMath::FRandRange(-MissileLaunchDelta, MissileLaunchDelta), 350.f);
    ImpactLocation = FireLocation;
    FlightDuration = LaunchTime;
    SpawnTime = GetWorld()->GetTimeSeconds();

    LaunchRotation = (MidLocation - StartLocation).Rotation().Quaternion();

    const FVector ImpactDirection = ImpactLocation - MidLocation;
    ImpactRotation = ImpactDirection.IsNearlyZero()
        ? LaunchRotation
        : ImpactDirection.Rotation().Quaternion();

    SetActorRotation(LaunchRotation);

    const UStaticMesh* WarningMesh = WarningSphere->GetStaticMesh();
    if (!IsValid(WarningMesh))
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: WarningSphere mesh is not configured."), *GetName());
        Destroy();
        return;
    }

    const double MeshRadius = WarningMesh->GetBounds().BoxExtent.GetMax();
    if (!FMath::IsFinite(MeshRadius) || MeshRadius <= 0.0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: WarningSphere mesh bounds are invalid."), *GetName());
        Destroy();
        return;
    }

    WarningSphere->SetWorldLocation(ImpactLocation);
    WarningSphere->SetWorldRotation(FRotator::ZeroRotator);
    WarningSphere->SetWorldScale3D(FVector(DamageRadius / MeshRadius));

    WarningMID = WarningSphere->CreateDynamicMaterialInstance(0);
    if (!IsValid(WarningMID.Get()))
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s: Failed to create warning material."), *GetName());
        Destroy();
        return;
    }

    WarningMID->SetScalarParameterValue(TEXT("Progress"), 0.f);
}

void ABossCircularMissile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bExploded)
    {
        return;
    }

    const auto EvaluateProgress = [](const UCurveFloat* Curve, float TimeAlpha)
        {
            const float T = FMath::Clamp(TimeAlpha, 0.f, 1.f);
            if (T <= 0.f || T >= 1.f)
            {
                return T;
            }

            const float Value = IsValid(Curve) ? Curve->GetFloatValue(T) : T;
            return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : T;
        };

    const double Elapsed = GetWorld()->GetTimeSeconds() - SpawnTime;

    if (IsValid(WarningMID.Get()))
    {
        const float Progress = FMath::Clamp(
            static_cast<float>(Elapsed / FlightDuration), 0.f, 1.f);
        WarningMID->SetScalarParameterValue(TEXT("Progress"), Progress);
    }

    const double MoveDuration = FlightDuration * 0.2;
    const double TurnDuration = FlightDuration * 0.4;
    const double DiveStart = MoveDuration + TurnDuration;
    const double DiveDuration = FlightDuration * 0.4;

    FVector NewLocation;
    FQuat NewRotation;

    if (Elapsed < MoveDuration)
    {
        const float Alpha = EvaluateProgress(
            LaunchMoveCurve.Get(), static_cast<float>(Elapsed / MoveDuration));
        NewLocation = FMath::Lerp(StartLocation, MidLocation, Alpha);
        NewRotation = LaunchRotation;
    }
    else if (Elapsed < DiveStart)
    {
        const float Alpha = EvaluateProgress(
            AimRotationCurve.Get(),
            static_cast<float>((Elapsed - MoveDuration) / TurnDuration));
        NewLocation = MidLocation;
        NewRotation = FQuat::Slerp(
            LaunchRotation, ImpactRotation, Alpha).GetNormalized();
    }
    else
    {
        const float Alpha = EvaluateProgress(
            ImpactMoveCurve.Get(),
            static_cast<float>((Elapsed - DiveStart) / DiveDuration));
        NewLocation = FMath::Lerp(MidLocation, ImpactLocation, Alpha);
        NewRotation = ImpactRotation;
    }

    SetActorLocationAndRotation(NewLocation, NewRotation, false);

    if (Elapsed >= FlightDuration)
    {
        Explode();
    }
}

void ABossCircularMissile::Explode()
{
    if (bExploded)
    {
        return;
    }
    bExploded = true;
    SetActorTickEnabled(false);
    SetActorLocation(ImpactLocation, false);

    WarningSphere->SetVisibility(false);
    TrailVFX->Deactivate();

    if (ExplosionVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, ExplosionVFX, ImpactLocation);
    }

    if (HasAuthority())
    {
        ApplyExplosionDamage();

        TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
        ObjectTypes.Add(
            UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

        TArray<AActor*> ActorsToIgnore;
        ActorsToIgnore.Add(this);

        TArray<AActor*> HitCores;
        UKismetSystemLibrary::SphereOverlapActors(
            this, ImpactLocation, DamageRadius, ObjectTypes,
            ABossActivationCore::StaticClass(),
            ActorsToIgnore, HitCores);

        for (AActor* Actor : HitCores)
        {
            if (ABossActivationCore* Core =
                Cast<ABossActivationCore>(Actor))
            {
                Core->HandleMissileHit(this);
            }
        }
    }
    Destroy();
}

void ABossCircularMissile::ApplyExplosionDamage()
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

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);
    ActorsToIgnore.Add(SourceBoss.Get());
    TArray<AActor*> OverlappingActors;

    UKismetSystemLibrary::SphereOverlapActors(
        this, ImpactLocation, DamageRadius, ObjectTypes,
        APlayerBase::StaticClass(), ActorsToIgnore, OverlappingActors);

    TSet<AActor*> DamagedActors;
    for (AActor* Actor : OverlappingActors)
    {
        APlayerBase* Player = Cast<APlayerBase>(Actor);
        if (!IsValid(Player) || !Player->IsPlayerControlled()
            || Player->IsDead() || DamagedActors.Contains(Player))
        {
            continue;
        }
        DamagedActors.Add(Player);

        UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent();
        if (!IsValid(TargetASC)
            || !TargetASC->HasAttributeSetForAttribute(
                UCombatAttributeSet::GetIncomingDamageAttribute()))
        {
            continue;
        }

        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddSourceObject(SourceBoss.Get());
        Context.AddOrigin(ImpactLocation);

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