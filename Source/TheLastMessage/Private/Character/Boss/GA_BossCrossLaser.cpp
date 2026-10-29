#include "Character/Boss/GA_BossCrossLaser.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossCircularMissile.h"
#include "Character/Player/PlayerBase.h"
#include "Framework/ActorPoolSubsystem.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

UGA_BossCrossLaser::UGA_BossCrossLaser()
{
    AttackType = EBossAttackType::CrossLaser;
    Phase2.Damage = 50.f;
    Phase2.BeamCount = 4;
    Phase2.Duration = 8.f;
    Phase2.MaxDegreesPerSecond = 90.f;
    Phase3.Damage = 70.f;
    Phase3.BeamCount = 4;
    Phase3.Duration = 10.f;
    Phase3.MaxDegreesPerSecond = 105.f;
    Phase3.bFireCircularMissiles = true;
}

bool UGA_BossCrossLaser::BuildRiseIntegral()
{
    RiseIntegral.SetNumZeroed(129);
    for (int32 I = 1; I <= 128; ++I)
    {
        const float A = RiseSpeedCurve ? RiseSpeedCurve->GetFloatValue((I - 1) / 128.f) : 1.f;
        const float B = RiseSpeedCurve ? RiseSpeedCurve->GetFloatValue(I / 128.f) : 1.f;
        if (!FMath::IsFinite(A) || !FMath::IsFinite(B)) return false;
        RiseIntegral[I] = RiseIntegral[I - 1] + (FMath::Max(0.f, A) + FMath::Max(0.f, B)) / 256.f;
    }
    const float Total = RiseIntegral.Last();
    if (!FMath::IsFinite(Total) || Total <= UE_SMALL_NUMBER) return false;
    for (float& Value : RiseIntegral) Value /= Total;
    return true;
}

float UGA_BossCrossLaser::RiseAlpha(float Time) const
{
    const float Sample = FMath::Clamp(Time, 0.f, 1.f) * 128.f;
    const int32 I = FMath::Min(FMath::FloorToInt(Sample), 127);
    return FMath::Lerp(RiseIntegral[I], RiseIntegral[I + 1], Sample - I);
}

void UGA_BossCrossLaser::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    if (!Boss || !Boss->HasAuthority() || !GetWorld() || !Anim
        || !Boss->GetCharacterMovement() || !IsValid(Boss->BossFlyLocator)
        || !IsValid(Boss->BossLaserCenter) || !JumpMontage || !ChargeMontage
        || !ReadyFlame || !FlyFlame || !WarningMaterial || !LaserMaterial
        || !WarningTriangleMaterial || !FiringTriangleMaterial
        || !Boss->GetMesh()->DoesSocketExist(FlameSocket))
    {
        Finish(true);
        return;
    }
    LockedPhase = Boss->GetBossPhase();
    Config = LockedPhase == EBossPhase::Phase1 ? Phase1
        : LockedPhase == EBossPhase::Phase2 ? Phase2 : Phase3;
    const float PositiveValues[] = { ExitDistance, ExitDuration, RiseDepth, RiseDuration,
TraceLength, BeamWidth, DamageInterval, Config.Duration, LaserImpactInterval };
    for (float Value : PositiveValues)
    {
        if (!FMath::IsFinite(Value) || Value <= 0.f) { Finish(true); return; }
    }
    const float NonNegativeValues[] = {
    JumpHeight, ExitDropHeight, OffMapDelay,
    Config.Damage, Config.MaxDegreesPerSecond
    };
    for (float Value : NonNegativeValues)
    {
        if (!FMath::IsFinite(Value) || Value < 0.f) { Finish(true); return; }
    }
    RiseEnd = Boss->BossFlyLocator->GetActorLocation();
    RiseStart = RiseEnd - FVector(0.f, 0.f, RiseDepth);
    if (Config.BeamCount < 1 || Config.BeamCount > 32 || !BuildRiseIntegral()
        || RiseStart.ContainsNaN() || RiseEnd.ContainsNaN()
        || RiseStart.Z <= GetWorld()->GetWorldSettings()->KillZ
        || Boss->GetActorLocation().Z - ExitDropHeight
        <= GetWorld()->GetWorldSettings()->KillZ
        || !DamageEffectClass || DamageEffectClass.GetDefaultObject()->DurationPolicy
        != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Error, TEXT("CrossLaser: invalid settings, damage GE or KillZ."));
        Finish(true);
        return;
    }
    if (Config.bFireCircularMissiles && (!MissileConfig.CircularMissileClass
        || MissileConfig.MissileCount < 1 || MissileSpawnOffset.ContainsNaN()
        || !FMath::IsFinite(MissileConfig.SpawnInterval) || MissileConfig.SpawnInterval <= 0.f
        || !FMath::IsFinite(MissileConfig.LaunchTime) || MissileConfig.LaunchTime < 0.01f
        || !FMath::IsFinite(MissileConfig.Damage) || MissileConfig.Damage < 0.f
        || !FMath::IsFinite(MissileConfig.DamageRadius) || MissileConfig.DamageRadius < 1.f))
    {
        Finish(true);
        return;
    }
    bDropLanded = false;
    DropHitPlayers.Reset();
    if (!ValidateFollowUp())
    {
        UE_LOG(LogTemp, Error, TEXT("CrossLaser: invalid follow-up settings."));
        Finish(true);
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { Finish(true); return; }

    SavedTransform = Boss->GetActorTransform();
    UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
    SavedMovementMode = Move->MovementMode;
    SavedCustomMode = Move->CustomMovementMode;
    bSavedPlaneConstraint = Move->bConstrainToPlane;
    bSavedCollision = Boss->GetActorEnableCollision();
    bSavedUseControllerYaw = Boss->bUseControllerRotationYaw;
    bOwnsMovement = true;
    Move->StopMovementImmediately();
    Move->DisableMovement();
    if (AAIController* AI = Cast<AAIController>(Boss->GetController())) AI->StopMovement();
    LockedTarget = Boss->GetCurrentTarget();
    NextDamageTime.Reset();
    Missiles.Reset();
    NextImpactTimes.Init(0.0, Config.BeamCount);
    Angle = 0.f;
    MissileCount = 0;
    Stage = EStage::WaitingNotify;
    StageStart = PreviousTime = GetWorld()->GetTimeSeconds();
    MontageInstanceID = INDEX_NONE;
    Boss->OnBossStateChanged.AddUniqueDynamic(this, &UGA_BossCrossLaser::OnStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive()) return;

    Geometry = NewObject<UProceduralMeshComponent>(Boss);
    Boss->AddInstanceComponent(Geometry);
    Geometry->SetAbsolute(true, true, true);
    Geometry->RegisterComponent();
    Geometry->SetWorldTransform(FTransform::Identity);
    Geometry->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Geometry->SetGenerateOverlapEvents(false);
    Geometry->SetCastShadow(false);
    Geometry->SetCanEverAffectNavigation(false);
    BoundAnim = Anim;
    Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UGA_BossCrossLaser::OnJumpNotify);
    SwitchFlame(ReadyFlame);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, JumpMontage, 1.f, NAME_None, true, 1.f, 0.f, true);
    if (!MontageTask) { Finish(true); return; }
    MontageTask->OnCompleted.AddDynamic(this, &UGA_BossCrossLaser::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossCrossLaser::OnMontageCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_BossCrossLaser::OnMontageCancelled);
    MontageTask->ReadyForActivation();
    if (!IsActive()) return;
    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(JumpMontage);
    if (!Instance) { Finish(true); return; }
    MontageInstanceID = Instance->GetInstanceID();
    GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this,
        &UGA_BossCrossLaser::UpdatePattern, 1.f / 60.f, true);
}

void UGA_BossCrossLaser::SwitchFlame(UNiagaraSystem* System)
{
    if (IsValid(Flame.Get()))
    {
        Flame->DeactivateImmediate();
        Flame->DestroyComponent();
    }
    Flame = nullptr;

    ABossMonster* Boss = GetBossOwner();
    if (!IsValid(Boss) || !System) return;

    Flame = UNiagaraFunctionLibrary::SpawnSystemAttached(
        System,
        Boss->GetMesh(),
        FlameSocket,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget,
        false,
        false,
        ENCPoolMethod::None,
        false);

    if (!IsValid(Flame.Get())) return;

    if (System == FlyFlame.Get())
    {
        Flame->SetAbsolute(false, true, false);
        Flame->SetWorldRotation(FRotator::ZeroRotator);
    }

    Flame->Activate(true);
}

void UGA_BossCrossLaser::OnJumpNotify(
    FName Name, const FBranchingPointNotifyPayload& Payload)
{
    ABossMonster* Boss = GetBossOwner();
    UAnimInstance* Anim = BoundAnim.Get();

    if (!IsActive() || bEnding || !IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(Anim) || !GetWorld()
        || Boss->GetBossState() != EBossState::Attacking
        || Boss->GetBossPhase() != LockedPhase
        || Payload.SkelMeshComponent != Boss->GetMesh())
    {
        return;
    }

    if (Stage != EStage::WaitingNotify
        || Name != FName(TEXT("JumpStart")))
    {
        return;
    }

    FAnimMontageInstance* Instance =
        Anim->GetInstanceForMontage(JumpMontage.Get());

    if (!Instance || Payload.MontageInstanceID != Instance->GetInstanceID()
        || (MontageInstanceID != INDEX_NONE
            && MontageInstanceID != Payload.MontageInstanceID))
    {
        return;
    }

    MontageInstanceID = Payload.MontageInstanceID;
    Boss->GetCharacterMovement()->SetPlaneConstraintEnabled(false);
    Boss->SetActorEnableCollision(false);
    Stage = EStage::Exiting;
    StageStart = GetWorld()->GetTimeSeconds();
}

void UGA_BossCrossLaser::UpdatePattern()
{
    if (!IsActive() || bEnding) return;
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = Cast<APlayerBase>(LockedTarget.Get());
    if (!Boss || !Boss->HasAuthority() || Boss->GetBossState() != EBossState::Attacking
        || Boss->GetBossPhase() != LockedPhase || !IsValid(Target) || Target->IsDead() || !Target->IsPlayerControlled()
        || Boss->GetCurrentTarget() != Target || !IsValid(Boss->BossLaserCenter)
        || !IsValid(Boss->BossFlyLocator)) {
        Finish(true); return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    const float Elapsed = static_cast<float>(Now - StageStart);
    switch (Stage)
    {
    case EStage::WaitingNotify:
        if (Elapsed > JumpMontage->GetPlayLength() + 1.f) Finish(true);
        break;
    case EStage::Exiting:
    {
        const float T = FMath::Clamp(Elapsed / ExitDuration, 0.f, 1.f);

        const float RiseRoot = FMath::Sqrt(JumpHeight);
        const float FallRoot = FMath::Sqrt(JumpHeight + ExitDropHeight);
        const float ArcScale = RiseRoot + FallRoot;

        const float ZOffset =
            2.f * RiseRoot * ArcScale * T
            - FMath::Square(ArcScale * T);

        const FVector P = SavedTransform.GetLocation()
            + FVector(0.f, -ExitDistance * T, ZOffset);

        Boss->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
        if (T >= 1.f) { Stage = EStage::OffMap; StageStart = Now; }
        break;
    }
    case EStage::OffMap:
        if (Elapsed >= OffMapDelay)
        {
            Boss->bUseControllerRotationYaw = false;
            Boss->SetActorRotation(FRotator(0.f, 90.f, 0.f),
                ETeleportType::TeleportPhysics);

            SwitchFlame(FlyFlame);
            Boss->SetActorLocation(RiseStart, false, nullptr, ETeleportType::TeleportPhysics);
            Stage = EStage::Rising;
            StageStart = Now;
        }
        break;
    case EStage::Rising:
    {
        const float T = FMath::Clamp(Elapsed / RiseDuration, 0.f, 1.f);
        Boss->SetActorLocation(FMath::Lerp(RiseStart, RiseEnd, RiseAlpha(T)),
            false, nullptr, ETeleportType::TeleportPhysics);

        if (T >= 1.f)
        {
            UAnimInstance* Anim = BoundAnim.Get();
            if (!IsValid(Anim) || !ChargeMontage)
            {
                Finish(true);
                return;
            }

            // Remove the old task's callbacks before replacing its montage.
            if (IsValid(MontageTask.Get()))
            {
                MontageTask->OnCompleted.RemoveAll(this);
                MontageTask->OnInterrupted.RemoveAll(this);
                MontageTask->OnCancelled.RemoveAll(this);
                MontageTask->EndTask();
            }
            MontageTask = nullptr;
            MontageInstanceID = INDEX_NONE;

            Stage = EStage::Warning;
            StageStart = Now;
            DrawLasers(false, Now);

            MontageTask =
                UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                    this, NAME_None, ChargeMontage.Get(),
                    1.f, NAME_None, true, 1.f, 0.f, true);

            if (!IsValid(MontageTask.Get()))
            {
                Finish(true);
                return;
            }

            MontageTask->OnCompleted.AddDynamic(
                this, &UGA_BossCrossLaser::OnMontageCompleted);
            MontageTask->OnInterrupted.AddDynamic(
                this, &UGA_BossCrossLaser::OnMontageCancelled);
            MontageTask->OnCancelled.AddDynamic(
                this, &UGA_BossCrossLaser::OnMontageCancelled);
            MontageTask->ReadyForActivation();

            if (!IsActive() || bEnding) return;

            FAnimMontageInstance* Instance =
                Anim->GetActiveInstanceForMontage(ChargeMontage.Get());
            if (!Instance)
            {
                Finish(true);
                return;
            }

            MontageInstanceID = Instance->GetInstanceID();
            Instance->bEnableAutoBlendOut = false;
        }
        break;
    }
    case EStage::Warning:
    {
        DrawLasers(false, Now);
        if (!IsActive() || bEnding) return;

        if (Elapsed >= 1.f)
        {
            Stage = EStage::Firing;
            StageStart = PreviousTime = NextMissileTime = Now;
            Angle = 0.f;


            if (IsValid(FiringEffect.Get())
                && !IsValid(FiringEffectComponent.Get()))
            {
                FiringEffectComponent =
                    UNiagaraFunctionLibrary::SpawnSystemAttached(
                        FiringEffect.Get(),
                        Boss->GetRootComponent(),
                        NAME_None,
                        FVector::ZeroVector,
                        FRotator::ZeroRotator,
                        EAttachLocation::SnapToTarget,
                        false,
                        true,
                        ENCPoolMethod::None,
                        true);
            }

            DrawLasers(true, Now);

            if (IsActive() && !bEnding && Config.bFireCircularMissiles)
            {
                SpawnMissile(Now);
            }
        }

        break;
    }
    case EStage::Firing:
    {
        const double End = FMath::Min(Now, StageStart + Config.Duration);
        // Substep angular integration so acceleration is independent of timer jitter.
        for (double T = PreviousTime; T < End; )
        {
            const double Step = FMath::Min(1.0 / 120.0, End - T);
            const float X = static_cast<float>((T + Step * 0.5 - StageStart) / Config.Duration);
            const float Raw = RotationSpeedCurve ? RotationSpeedCurve->GetFloatValue(X) : X;
            if (!FMath::IsFinite(Raw)) { Finish(true); return; }
            Angle = FMath::Fmod(Angle + Config.MaxDegreesPerSecond * FMath::Clamp(Raw, 0.f, 1.f) * Step, 360.f);
            T += Step;
        }
        PreviousTime = End;
        if (Elapsed >= Config.Duration)
        {
            BeginLaserFadeOut(Now);
            return;
        }
        DrawLasers(true, Now);
        if (IsActive() && Config.bFireCircularMissiles && MissileCount < MissileConfig.MissileCount
            && Now >= NextMissileTime) SpawnMissile(Now);
        break;
    }
    case EStage::LaserFadeOut:
    case EStage::FollowRise:
    case EStage::DropWarning:
    case EStage::Dropping:
        UpdateFollowUp(Now);
        break;
    }
}

void UGA_BossCrossLaser::WriteSection(int32 Index, const TArray<FVector>& V, UMaterialInterface* Material)
{
    if (!Geometry || V.Num() < 3) return;
    const FVector Normal = FVector::CrossProduct(V[1] - V[0], V[2] - V[0]).GetSafeNormal();
    TArray<FVector> Normals;
    Normals.Init(Normal, V.Num());
    TArray<FVector2D> UV = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1) };
    TArray<int32> Indices = { 0, 1, 2 };
    if (V.Num() == 4) { UV.Add(FVector2D(0, 1)); Indices.Append({ 0, 2, 3 }); }
    const TArray<FColor> Colors;
    const TArray<FProcMeshTangent> Tangents;
    if (!Geometry->GetProcMeshSection(Index))
        Geometry->CreateMeshSection(Index, V, Indices, Normals, UV, Colors, Tangents, false);
    else
        Geometry->UpdateMeshSection(Index, V, Normals, UV, Colors, Tangents);
    Geometry->SetMaterial(Index, Material);
}

void UGA_BossCrossLaser::DrawLasers(bool bDamage, double Now)
{
    ABossMonster* Boss = GetBossOwner();
    if (!Boss || !Geometry) return;

    const float FadeDuration = bDamage ? FiringFadeInDuration : 1.f;
    if (!FMath::IsFinite(FadeDuration) || FadeDuration <= 0.f)
    {
        Finish(true);
        return;
    }

    const float Progress = FMath::Clamp(
        static_cast<float>((Now - StageStart) / FadeDuration),
        0.f, 1.f);

    UCurveFloat* OpacityCurve =
        bDamage ? FiringOpacityCurve.Get() : WarningOpacityCurve.Get();

    const float RawOpacity = OpacityCurve
        ? OpacityCurve->GetFloatValue(Progress)
        : Progress;

    if (!FMath::IsFinite(RawOpacity))
    {
        Finish(true);
        return;
    }

    TObjectPtr<UMaterialInstanceDynamic>& ActiveMID =
        bDamage ? FiringLaserMID : WarningLaserMID;

    if (!IsValid(ActiveMID.Get()))
    {
        UMaterialInterface* ParentMaterial =
            bDamage ? LaserMaterial.Get() : WarningMaterial.Get();

        ActiveMID = UMaterialInstanceDynamic::Create(ParentMaterial, this);
        if (!IsValid(ActiveMID.Get()))
        {
            Finish(true);
            return;
        }
    }

    TObjectPtr<UMaterialInstanceDynamic>& ActiveTriangleMID =
        bDamage ? FiringTriangleMID : WarningTriangleMID;

    if (!IsValid(ActiveTriangleMID.Get()))
    {
        UMaterialInterface* ParentMaterial = bDamage
            ? FiringTriangleMaterial.Get()
            : WarningTriangleMaterial.Get();

        ActiveTriangleMID =
            UMaterialInstanceDynamic::Create(ParentMaterial, this);

        if (!IsValid(ActiveTriangleMID.Get()))
        {
            Finish(true);
            return;
        }
    }

    const float Opacity = FMath::Clamp(RawOpacity, 0.f, 1.f);

    ActiveMID->SetScalarParameterValue(TEXT("LaserOpacity"), Opacity);
    ActiveTriangleMID->SetScalarParameterValue(TEXT("LaserOpacity"), Opacity);
    const FVector Center = Boss->BossLaserCenter->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BossCrossLaser), false, Boss);
    Query.AddIgnoredActor(Boss->BossLaserCenter);
    Query.AddIgnoredActor(Boss->BossFlyLocator);
    TSet<TWeakObjectPtr<AActor>> Players;
    for (int32 I = 0; I < Config.BeamCount; ++I)
    {
        const FVector Direction = FVector::UpVector.RotateAngleAxis(
            Angle + I * (360.f / Config.BeamCount), FVector::YAxisVector);
        FVector End = Center + Direction * TraceLength;
        TArray<FHitResult> Hits;
        // Do not gate on the bool return: overlap-only hits may return false.
        GetWorld()->LineTraceMultiByChannel(Hits, Center, End, LaserTraceChannel, Query);
        for (const FHitResult& Hit : Hits)
        {
            if (APlayerBase* Player = Cast<APlayerBase>(Hit.GetActor()))
                Players.Add(TWeakObjectPtr<AActor>(Player));
            if (Hit.bBlockingHit)
            {
                End = Hit.ImpactPoint;

                if (Stage == EStage::Firing
                    && IsValid(LaserImpactEffect.Get())
                    && !Cast<APlayerBase>(Hit.GetActor())
                    && NextImpactTimes.IsValidIndex(I)
                    && Now >= NextImpactTimes[I])
                {
                    NextImpactTimes[I] = Now + LaserImpactInterval;

                    const FVector Normal = Hit.ImpactNormal;
                    const FVector EffectLocation =
                        Hit.ImpactPoint + Normal * 2.f;
                    const FRotator EffectRotation =
                        FRotationMatrix::MakeFromZ(Normal).Rotator();

                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        GetWorld(),
                        LaserImpactEffect.Get(),
                        EffectLocation,
                        EffectRotation,
                        FVector::OneVector,
                        true,
                        true,
                        ENCPoolMethod::AutoRelease,
                        true);

                    if (!IsActive() || bEnding) return;
                }

                break;
            }
        }
        const FVector Side = FVector::CrossProduct(Direction, FVector::YAxisVector) * BeamWidth * 0.5f;
        WriteSection(I * 2, { Center - Side, Center + Side, End + Side, End - Side },
            ActiveMID.Get());
        WriteSection(I * 2 + 1, { Boss->GetActorLocation(), Center, End },
            ActiveTriangleMID.Get());
    }
    if (bDamage)
    {
        for (const TWeakObjectPtr<AActor>& Player : Players)
        {
            DamagePlayer(Player.Get(), Now);
            if (!IsActive() || bEnding) return;
        }
    }
}

void UGA_BossCrossLaser::DamagePlayer(AActor* Target, double Now)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = Cast<APlayerBase>(Target);
    if (!Boss || !Boss->HasAuthority() || !IsValid(Player) || Player->IsDead()) return;
    const TWeakObjectPtr<AActor> Key(Target);
    const double* Next = NextDamageTime.Find(Key);
    if (Next && Now < *Next) return;
    UAbilitySystemComponent* Source = Boss->GetAbilitySystemComponent();
    UAbilitySystemComponent* Dest = Player->GetAbilitySystemComponent();
    if (!Source || !Dest || !Dest->HasAttributeSetForAttribute(
        UCombatAttributeSet::GetIncomingDamageAttribute())) return;
    FGameplayEffectContextHandle Context = Source->MakeEffectContext();
    Context.AddSourceObject(Boss);
    FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
    if (!Spec.IsValid()) return;
    Spec.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_Damage, Config.Damage);
    NextDamageTime.Add(Key, Now + DamageInterval);
    Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Dest);
}

void UGA_BossCrossLaser::SpawnMissile(double Now)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = Cast<APlayerBase>(LockedTarget.Get());
    UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>();
    if (!Boss || !Target || !Pool) { Finish(true); return; }
    const bool bFirstInPair = MissileCount % 2 == 0;
    if (bFirstInPair)
    {
        PairTargetA = Target->GetActorLocation();
        FVector Velocity = Target->GetVelocity();
        Velocity.Y = 0.f;
        PairTargetB = PairTargetA + Velocity * MissileConfig.LaunchTime;
    }
    const FVector Location = Boss->GetActorLocation() + Boss->GetActorRotation().RotateVector(MissileSpawnOffset);
    ABossCircularMissile* Missile = Cast<ABossCircularMissile>(Pool->AcquireActor(
        MissileConfig.CircularMissileClass, FTransform(FRotator::ZeroRotator, Location), Boss, Boss));
    if (!Missile) { Finish(true); return; }
    const TWeakObjectPtr<ABossCircularMissile> WeakMissile(Missile);
    Missile->OnMissileReturned.BindWeakLambda(this, [this, WeakMissile]()
        {
            Missiles.Remove(WeakMissile);
        });
    Missiles.Add(WeakMissile);
    Missile->Damage = MissileConfig.Damage;
    Missile->LaunchTime = MissileConfig.LaunchTime;
    Missile->DamageRadius = MissileConfig.DamageRadius;
    Missile->FireLocation = bFirstInPair ? PairTargetA : PairTargetB;
    Missile->StartMissile();
    if (!IsActive() || bEnding) return;
    if (!Missile->IsMissileActive()) { Finish(true); return; }
    ++MissileCount;
    NextMissileTime = Now + MissileConfig.SpawnInterval;
}


bool UGA_BossCrossLaser::ValidateFollowUp()
{
    if (!FollowRiseMontage || !DropMontage || !DropWarningMesh || !DropWarningMaterial)
        return false;
    const float Positive[] = { FiringFadeInDuration, FollowRiseHeight, FollowRiseDuration,
        DropDamageDiameter, DropWarningHeight, DropWarningDuration, DropSpeed };
    for (float V : Positive)
        if (!FMath::IsFinite(V) || V <= 0.f) return false;
    const float NonNegative[] = { DropDamage, DropKnockbackHorizontal, DropKnockbackUp };
    for (float V : NonNegative)
        if (!FMath::IsFinite(V) || V < 0.f) return false;

    FollowRiseIntegral.SetNumZeroed(129);
    for (int32 I = 1; I <= 128; ++I)
    {
        const float A = FollowRiseSpeedCurve
            ? FollowRiseSpeedCurve->GetFloatValue((I - 1) / 128.f) : 1.f;
        const float B = FollowRiseSpeedCurve
            ? FollowRiseSpeedCurve->GetFloatValue(I / 128.f) : 1.f;
        if (!FMath::IsFinite(A) || !FMath::IsFinite(B)) return false;
        FollowRiseIntegral[I] = FollowRiseIntegral[I - 1]
            + (FMath::Max(0.f, A) + FMath::Max(0.f, B)) / 256.f;
    }
    const float Total = FollowRiseIntegral.Last();
    if (!FMath::IsFinite(Total) || Total <= UE_SMALL_NUMBER) return false;
    for (float& V : FollowRiseIntegral) V /= Total;

    const FBoxSphereBounds Bounds = DropWarningMesh->GetBounds();
    return !Bounds.Origin.ContainsNaN() && !Bounds.BoxExtent.ContainsNaN()
        && Bounds.BoxExtent.X > 0.f && Bounds.BoxExtent.Y > 0.f && Bounds.BoxExtent.Z > 0.f;
}

float UGA_BossCrossLaser::GetFollowRiseAlpha(float T) const
{
    const float Sample = FMath::Clamp(T, 0.f, 1.f) * 128.f;
    const int32 I = FMath::Min(FMath::FloorToInt(Sample), 127);
    return FMath::Lerp(FollowRiseIntegral[I], FollowRiseIntegral[I + 1], Sample - I);
}

bool UGA_BossCrossLaser::PlayFollowMontage(UAnimMontage* Montage)
{
    UAnimInstance* Anim = BoundAnim.Get();
    if (!IsValid(Anim) || !Montage) return false;
    if (IsValid(MontageTask.Get()))
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
        MontageTask->EndTask();
    }
    MontageTask = nullptr;
    MontageInstanceID = INDEX_NONE;
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, Montage, 1.f, NAME_None, true, 1.f, 0.f, true);
    if (!MontageTask) return false;
    MontageTask->OnCompleted.AddDynamic(this, &UGA_BossCrossLaser::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossCrossLaser::OnMontageCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_BossCrossLaser::OnMontageCancelled);
    MontageTask->ReadyForActivation();
    if (!IsActive() || bEnding) return false;
    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
    if (!Instance) return false;
    MontageInstanceID = Instance->GetInstanceID();
    Instance->bEnableAutoBlendOut = false;
    return true;
}

void UGA_BossCrossLaser::BeginLaserFadeOut(double Now)
{
    Stage = EStage::LaserFadeOut;
    StageStart = Now;
    FadeStartProgress = FMath::Clamp(Config.Duration / FiringFadeInDuration, 0.f, 1.f);
    if (IsValid(FiringEffectComponent.Get()))
    {
        FiringEffectComponent->DeactivateImmediate();
        FiringEffectComponent->DestroyComponent();
    }
    FiringEffectComponent = nullptr;
    // Stop the firing batch; returned missiles remove themselves from Missiles.
    const TArray<TWeakObjectPtr<ABossCircularMissile>> Remaining = MoveTemp(Missiles);
    Missiles.Reset();
    for (const TWeakObjectPtr<ABossCircularMissile>& Entry : Remaining)
        if (ABossCircularMissile* Missile = Entry.Get()) Missile->ReturnToPool();
}

bool UGA_BossCrossLaser::CreateDropWarning(double Now)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = Cast<APlayerBase>(LockedTarget.Get());
    if (!Boss || !IsValid(Target) || !DropWarningMesh || !DropWarningMaterial) return false;

    const FVector TraceStart = Target->GetActorLocation();
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CrossLaserDropGround), false, Boss);
    Query.AddIgnoredActor(Target);
    FHitResult GroundHit;
    if (!GetWorld()->LineTraceSingleByObjectType(GroundHit, TraceStart,
        TraceStart - FVector(0.f, 0.f, 10000.f), Objects, Query)
        || GroundHit.ImpactNormal.Z < 0.5f)
    {
        UE_LOG(LogTemp, Warning, TEXT("CrossLaser: no suitable ground for drop."));
        return false;
    }
    DropGroundPoint = GroundHit.ImpactPoint;
    const float HalfHeight = Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    DropEnd = DropGroundPoint + FVector(0.f, 0.f, HalfHeight + 2.f);
    if (DropEnd.ContainsNaN() || DropEnd.Z >= Boss->GetActorLocation().Z
        || DropEnd.Z <= GetWorld()->GetWorldSettings()->KillZ) return false;

    ClearDropWarning();
    DropWarningComponent = NewObject<UStaticMeshComponent>(Boss);
    Boss->AddInstanceComponent(DropWarningComponent);
    DropWarningComponent->SetAbsolute(true, true, true);
    DropWarningComponent->SetStaticMesh(DropWarningMesh);
    DropWarningComponent->SetMaterial(0, DropWarningMaterial);
    DropWarningComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DropWarningComponent->SetGenerateOverlapEvents(false);
    DropWarningComponent->SetCastShadow(false);
    DropWarningComponent->SetCanEverAffectNavigation(false);
    DropWarningComponent->RegisterComponent();

    const FBoxSphereBounds Bounds = DropWarningMesh->GetBounds();
    const FVector Scale(DropDamageDiameter / (2.f * Bounds.BoxExtent.X),
        DropDamageDiameter / (2.f * Bounds.BoxExtent.Y),
        DropWarningHeight / (2.f * Bounds.BoxExtent.Z));
    const FVector Center = DropGroundPoint + FVector(0.f, 0.f, DropWarningHeight * 0.5f + 2.f);
    DropWarningComponent->SetWorldRotation(FRotator::ZeroRotator);
    DropWarningComponent->SetWorldScale3D(Scale);
    DropWarningComponent->SetWorldLocation(Center - Bounds.Origin * Scale);
    DropWarningMID = DropWarningComponent->CreateDynamicMaterialInstance(0);
    if (!DropWarningMID) return false;
    DropWarningMID->SetScalarParameterValue(TEXT("Progress"), 0.f);
    Stage = EStage::DropWarning;
    StageStart = Now;
    return true;
}

void UGA_BossCrossLaser::SweepDropPlayers(const FVector& From, const FVector& To)
{
    ABossMonster* Boss = GetBossOwner();
    if (!Boss || !Boss->HasAuthority()) return;
    const float Radius = DropDamageDiameter * 0.5f;
    const float HalfHeight = FMath::Max(Radius,
        Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CrossLaserDropPlayers), false, Boss);
    Query.bFindInitialOverlaps = true;
    // The query shape only overlaps Pawn objects; terrain cannot truncate this damage query.
    FCollisionResponseParams Responses;
    Responses.CollisionResponse.SetAllChannels(ECR_Ignore);
    Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Overlap);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByChannel(Hits, From, To, FQuat::Identity,
        LaserTraceChannel, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query, Responses);

    for (const FHitResult& Hit : Hits)
    {
        APlayerBase* Player = Cast<APlayerBase>(Hit.GetActor());
        if (!IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead()
            || Hit.GetComponent() != Player->GetCapsuleComponent()) continue;
        const TWeakObjectPtr<AActor> Key(Player);
        if (DropHitPlayers.Contains(Key)) continue;
        DropHitPlayers.Add(Key);

        UAbilitySystemComponent* Source = Boss->GetAbilitySystemComponent();
        UAbilitySystemComponent* Dest = Player->GetAbilitySystemComponent();
        if (!Source || !Dest || !Dest->HasAttributeSetForAttribute(
            UCombatAttributeSet::GetIncomingDamageAttribute())) continue;
        const FVector ContactBossLocation = FMath::Lerp(From, To, Hit.Time);
        const float Sign = Player->GetActorLocation().X < ContactBossLocation.X ? -1.f : 1.f;
        FGameplayEffectContextHandle Context = Source->MakeEffectContext();
        Context.AddSourceObject(Boss);
        Context.AddOrigin(ContactBossLocation);
        Context.AddHitResult(Hit);
        FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), Context);
        if (!Spec.IsValid()) continue;
        Spec.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_Damage, DropDamage);
        const FActiveGameplayEffectHandle Applied = Source->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Dest);
        if (!IsActive() || bEnding) return;
        if (Applied.WasSuccessfullyApplied() && IsValid(Player) && !Player->IsDead())
            Player->LaunchCharacter(FVector(Sign * DropKnockbackHorizontal, 0.f, DropKnockbackUp), true, true);
    }
}

void UGA_BossCrossLaser::ClearDropWarning()
{
    if (IsValid(DropWarningComponent.Get()))
    {
        if (ABossMonster* Boss = GetBossOwner()) Boss->RemoveInstanceComponent(DropWarningComponent);
        DropWarningComponent->DestroyComponent();
    }
    DropWarningComponent = nullptr;
    DropWarningMID = nullptr;
}

void UGA_BossCrossLaser::UpdateFollowUp(double Now)
{
    ABossMonster* Boss = GetBossOwner();
    if (!Boss || !IsActive() || bEnding) return;
    const float Elapsed = static_cast<float>(Now - StageStart);
    switch (Stage)
    {
    case EStage::LaserFadeOut:
    {
        const float T = FMath::Clamp(Elapsed / FiringFadeInDuration, 0.f, 1.f);
        const float X = FadeStartProgress * (1.f - T);
        const float Raw = FiringOpacityCurve ? FiringOpacityCurve->GetFloatValue(X) : X;
        if (!FMath::IsFinite(Raw)) { Finish(true); return; }
        const float Opacity = T >= 1.f ? 0.f : FMath::Clamp(Raw, 0.f, 1.f);
        if (FiringLaserMID) FiringLaserMID->SetScalarParameterValue(TEXT("LaserOpacity"), Opacity);
        if (FiringTriangleMID) FiringTriangleMID->SetScalarParameterValue(TEXT("LaserOpacity"), Opacity);
        if (T >= 1.f)
        {
            if (Geometry)
            {
                Boss->RemoveInstanceComponent(Geometry);
                Geometry->DestroyComponent();
                Geometry = nullptr;
            }
            FollowRiseStart = Boss->GetActorLocation();
            FollowRiseEnd = FollowRiseStart + FVector(0.f, 0.f, FollowRiseHeight);
            Stage = EStage::FollowRise;
            StageStart = Now;
            if (!PlayFollowMontage(FollowRiseMontage)) Finish(true);
        }
        break;
    }
    case EStage::FollowRise:
    {
        const float T = FMath::Clamp(Elapsed / FollowRiseDuration, 0.f, 1.f);
        Boss->SetActorLocation(FMath::Lerp(FollowRiseStart, FollowRiseEnd, GetFollowRiseAlpha(T)),
            false, nullptr, ETeleportType::TeleportPhysics);
        if (T >= 1.f && !CreateDropWarning(Now)) Finish(true);
        break;
    }
    case EStage::DropWarning:
    {
        if (!DropWarningMID) { Finish(true); return; }
        DropWarningMID->SetScalarParameterValue(TEXT("Progress"),
            FMath::Clamp(Elapsed / DropWarningDuration, 0.f, 1.f));
        if (Elapsed >= DropWarningDuration)
        {
            DropStart = FVector(
                DropEnd.X,
                DropEnd.Y,
                Boss->GetActorLocation().Z);

            if (DropStart.ContainsNaN() || DropStart.Z <= DropEnd.Z)
            {
                Finish(true);
                return;
            }

            DropDuration = (DropStart.Z - DropEnd.Z) / DropSpeed;
            if (!FMath::IsFinite(DropDuration) || DropDuration <= 0.f)
            {
                Finish(true);
                return;
            }

            Boss->SetActorLocation(
                DropStart, false, nullptr,
                ETeleportType::TeleportPhysics);
            DropHitPlayers.Reset();
            if (IsValid(Flame.Get()))
            {
                Flame->DeactivateImmediate();
                Flame->DestroyComponent();
            }
            Flame = nullptr;
            Stage = EStage::Dropping;
            StageStart = Now;
            if (!PlayFollowMontage(DropMontage)) Finish(true);
        }
        break;
    }
    case EStage::Dropping:
    {
        const float T = FMath::Clamp(Elapsed / DropDuration, 0.f, 1.f);
        const FVector From = Boss->GetActorLocation();
        const FVector To = FMath::Lerp(DropStart, DropEnd, T);

        Boss->SetActorLocation(
            To, false, nullptr,
            ETeleportType::TeleportPhysics);

        SweepDropPlayers(From, To);
        if (!IsActive() || bEnding) return;

        if (T >= 1.f)
        {
            bDropLanded = true;
            ClearDropWarning();
            Finish(false);
        }

        break;
    }
    default:
        break;
    }
}

void UGA_BossCrossLaser::OnMontageCompleted()
{
    if (!IsActive() || bEnding) return;

    if (Stage == EStage::WaitingNotify)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CrossLaser: montage completed before its required notify. Stage=%d"),
            static_cast<int32>(Stage));

        Finish(true);
    }
}

void UGA_BossCrossLaser::OnMontageCancelled()
{
    if (IsActive() && !bEnding) Finish(true);
}

void UGA_BossCrossLaser::OnStateChanged(EBossState NewState)
{
    if (IsActive() && !bEnding && NewState != EBossState::Attacking) Finish(true);
}

void UGA_BossCrossLaser::Finish(bool bCancelled)
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, bCancelled);
}

void UGA_BossCrossLaser::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bEnding || !IsEndAbilityValid(Handle, ActorInfo)) return;
    TGuardValue<bool> Guard(bEnding, true);
    ClearDropWarning();
    DropHitPlayers.Reset();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    if (UAnimInstance* Anim = BoundAnim.Get())
        Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UGA_BossCrossLaser::OnJumpNotify);
    BoundAnim.Reset();
    if (MontageTask)
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
    }
    MontageTask = nullptr;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageStop(0.1f);
        ASC->ClearAnimatingAbility(this);
    }
    if (IsValid(FiringEffectComponent.Get()))
    {
        FiringEffectComponent->DeactivateImmediate();
        FiringEffectComponent->DestroyComponent();
    }
    FiringEffectComponent = nullptr;
    if (Flame) { Flame->DeactivateImmediate(); Flame->DestroyComponent(); Flame = nullptr; }
    if (Geometry)
    {
        if (ABossMonster* Owner = GetBossOwner()) Owner->RemoveInstanceComponent(Geometry);
        Geometry->DestroyComponent();
        Geometry = nullptr;
    }
    WarningLaserMID = nullptr;
    FiringLaserMID = nullptr;
    WarningTriangleMID = nullptr;
    FiringTriangleMID = nullptr;
    const TArray<TWeakObjectPtr<ABossCircularMissile>> RemainingMissiles = MoveTemp(Missiles);
    Missiles.Reset();
    for (const TWeakObjectPtr<ABossCircularMissile>& Entry : RemainingMissiles)
        if (ABossCircularMissile* Missile = Entry.Get()) Missile->ReturnToPool();
    ABossMonster* Boss = GetBossOwner();
    if (Boss)
    {
        Boss->OnBossStateChanged.RemoveDynamic(this, &UGA_BossCrossLaser::OnStateChanged);
        if (bOwnsMovement)
        {
            if (bWasCancelled || !bDropLanded)
            {
                Boss->SetActorTransform(
                    SavedTransform, false, nullptr,
                    ETeleportType::TeleportPhysics);
            }
            else
            {
                Boss->SetActorRotation(
                    SavedTransform.GetRotation(),
                    ETeleportType::TeleportPhysics);
            }
            Boss->bUseControllerRotationYaw = bSavedUseControllerYaw;
            Boss->SetActorEnableCollision(bSavedCollision);
            UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
            Move->StopMovementImmediately();
            Move->SetPlaneConstraintEnabled(bSavedPlaneConstraint);
            // Existing puzzle/cinematic code stops velocity but does not restore movement mode.
            if (Boss->GetBossState() != EBossState::Dead)
                Move->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode), SavedCustomMode);
            bOwnsMovement = false;
        }
        if (Boss->GetBossState() == EBossState::Attacking) Boss->SetBossState(EBossState::Combat);
    }
    NextImpactTimes.Reset();
    LockedTarget.Reset();
    NextDamageTime.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}