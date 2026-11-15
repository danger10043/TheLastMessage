#include "Character/Boss/GA_BossJumpAttack.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttackTrailComponent.h"
#include "Character/Boss/BossCharacterMovementComponent.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UGA_BossJumpAttack::UGA_BossJumpAttack()
{
    AttackType = EBossAttackType::JumpAttack;
}

bool UGA_BossJumpAttack::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    const UBossCharacterMovementComponent* Move = IsValid(Boss)
        ? Cast<UBossCharacterMovementComponent>(Boss->GetCharacterMovement()) : nullptr;
    return IsValid(Boss) && Boss->GetBossPhase() == EBossPhase::Phase3
        && Move && Move->IsMovingOnGround() && !Move->bJumpAttackActive
        && Super::CanActivateAbility(
            Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_BossJumpAttack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    bStartedJump = false;
    bLanded = false;
    bSavedSettings = false;
    MontageInstanceID = INDEX_NONE;
    HitPlayers.Reset();

    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;
    UBossCharacterMovementComponent* Move = IsValid(Boss)
        ? Cast<UBossCharacterMovementComponent>(Boss->GetCharacterMovement()) : nullptr;
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    const UGameplayEffect* DamageDefaults = DamageEffectClass
        ? DamageEffectClass.GetDefaultObject() : nullptr;

    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || Boss->GetBossPhase() != EBossPhase::Phase3
        || !IsValid(Player) || Player->IsDead() || !Player->IsPlayerControlled()
        || !Move || !Move->IsMovingOnGround() || Move->bJumpAttackActive
        || !Anim || !AttackMontage || !ReadyEffect || !StartEffect
        || !JumpStartGroundEffect
        || !DamageDefaults
        || DamageDefaults->DurationPolicy != EGameplayEffectDurationType::Instant
        || !Boss->GetMesh()->DoesSocketExist(TEXT("BossFlameBackSocket"))
        || !Move->GetGravityDirection().Equals(FVector(0.f, 0.f, -1.f), 0.001f))
    {
        CancelPattern();
        return;
    }

    for (const FName Section : { FName(TEXT("JumpReady")),
        FName(TEXT("JumpStart")), FName(TEXT("JumpLoop")) })
    {
        if (AttackMontage->GetSectionIndex(Section) == INDEX_NONE)
        {
            CancelPattern();
            return;
        }
    }
    for (const float Value : { ApexHeightThreshold, PreferredJumpHeight,
        MaxJumpHeight, MaxHorizontalSpeed, FloorSearchDepth })
    {
        if (!FMath::IsFinite(Value) || Value <= 0.f)
        {
            CancelPattern();
            return;
        }
    }
    if (PreferredJumpHeight > MaxJumpHeight
        || !FMath::IsFinite(Damage) || Damage < 0.f
        || !FMath::IsFinite(JumpGravityMultiplier) || JumpGravityMultiplier < 1.f
        || !FMath::IsFinite(Move->GravityScale * JumpGravityMultiplier)
        || !FMath::IsFinite(KnockbackHorizontalSpeed) || KnockbackHorizontalSpeed < 0.f
        || !FMath::IsFinite(KnockbackUpSpeed) || KnockbackUpSpeed < 0.f)
    {
        CancelPattern();
        return;
    }

    TargetSnapshot = Player->GetActorLocation();
    TargetHalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    bUseApex = TargetSnapshot.Z - Boss->GetActorLocation().Z >= ApexHeightThreshold;
    if (TargetSnapshot.ContainsNaN() || (!bUseApex && !FindLandingCenter(LandingCenter)))
    {
        CancelPattern();
        return;
    }
    FVector InitialVelocity;
    if (!BuildLaunchVelocity(InitialVelocity)
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelPattern();
        return;
    }

    UCapsuleComponent* Capsule = Boss->GetCapsuleComponent();
    SavedSoftResponse = Capsule->GetCollisionResponseToChannel(ECC_GameTraceChannel1);
    SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
    bSavedOverlapEvents = Capsule->GetGenerateOverlapEvents();
    SavedAirControl = Move->AirControl;
    SavedFriction = Move->FallingLateralFriction;
    SavedBraking = Move->BrakingDecelerationFalling;
    SavedAcceleration = Move->MaxAcceleration;
    SavedMaxSpeed = Move->MaxWalkSpeed;
    SavedGravityScale = Move->GravityScale;
    SavedRootMotionMode = Anim->RootMotionMode;
    BoundAnim = Anim;
    bSavedSettings = true;

    if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
    {
        AI->StopMovement();
    }
    Move->StopMovementImmediately();
    Move->MaxAcceleration = 0.f;
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    Boss->OnBossStateChanged.AddUniqueDynamic(this, &ThisClass::OnStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive() || bEnding)
    {
        return;
    }

    Boss->LandedDelegate.AddUniqueDynamic(this, &ThisClass::OnLanded);
    Capsule->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnOverlap);
    Anim->OnMontageSectionChanged.AddUniqueDynamic(this, &ThisClass::OnSectionChanged);

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage.Get(), 1.f, TEXT("JumpReady"),
        true, 1.f, 0.f, true);
    if (!MontageTask)
    {
        CancelPattern();
        return;
    }
    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageEnded);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageEnded);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageEnded);
    SetEffect(ReadyEffect.Get());
    MontageTask->ReadyForActivation();
    if (!IsActive() || bEnding)
    {
        return;
    }

    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(AttackMontage.Get());
    if (!Instance)
    {
        CancelPattern();
        return;
    }
    MontageInstanceID = Instance->GetInstanceID();
    Anim->Montage_SetNextSection(TEXT("JumpReady"), TEXT("JumpStart"), AttackMontage.Get());
    Anim->Montage_SetNextSection(TEXT("JumpStart"), TEXT("JumpLoop"), AttackMontage.Get());
    Anim->Montage_SetNextSection(TEXT("JumpLoop"), TEXT("JumpLoop"), AttackMontage.Get());
    if (!bStartedJump)
    {
        float ReadyStart = 0.f;
        float ReadyEnd = 0.f;
        AttackMontage->GetSectionStartAndEndTime(
            AttackMontage->GetSectionIndex(TEXT("JumpReady")), ReadyStart, ReadyEnd);
        const float EffectiveRate = Instance->GetPlayRate() * AttackMontage->RateScale;
        if (!FMath::IsFinite(EffectiveRate) || EffectiveRate <= 0.f
            || !FMath::IsFinite(FacingTurnDuration) || FacingTurnDuration <= 0.f)
        {
            CancelPattern();
            return;
        }
        const float ReadyRemaining = (ReadyEnd - Instance->GetPosition()) / EffectiveRate;
        if (!FacingTurn.Start(Boss, TargetSnapshot.X, FacingTurnSpeedCurve.Get(),
            FMath::Min(FacingTurnDuration, ReadyRemaining), GetWorld()->GetTimeSeconds()))
        {
            CancelPattern();
            return;
        }
    }
    GetWorld()->GetTimerManager().SetTimer(
        ContextTimer, this, &ThisClass::CheckContext, 0.02f, true);
    if (PatternStartSound)
    {
        Boss->MulticastGunBurstSound(PatternStartSound.Get(), Boss->GetActorLocation());
    }
    if (Anim->Montage_GetCurrentSection(AttackMontage.Get()) != TEXT("JumpReady"))
    {
        StartJump();
    }
}

bool UGA_BossJumpAttack::FindLandingCenter(FVector& OutCenter) const
{
    ABossMonster* Boss = GetBossOwner();
    const UCapsuleComponent* Capsule = Boss->GetCapsuleComponent();
    const UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
    FVector From = TargetSnapshot;
    From.Z += Capsule->GetScaledCapsuleHalfHeight() - TargetHalfHeight + 5.f;
    FVector To = From;
    To.Z -= FloorSearchDepth;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BossJumpLanding), false, Boss);
    Params.AddIgnoredActor(Boss->GetCurrentTarget());
    FHitResult Hit;
    const bool bHit = GetWorld()->SweepSingleByObjectType(
        Hit, From, To, FQuat::Identity, Objects,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
            Capsule->GetScaledCapsuleHalfHeight()), Params);
    if (!bHit || Hit.bStartPenetrating || !Move->IsWalkable(Hit))
    {
        return false;
    }
    OutCenter = Hit.Location;
    return !OutCenter.ContainsNaN();
}

bool UGA_BossJumpAttack::BuildLaunchVelocity(FVector& OutVelocity) const
{
    ABossMonster* Boss = GetBossOwner();
    const UCharacterMovementComponent* Move = Boss->GetCharacterMovement();
    const double Gravity = -Move->GetGravityZ() * static_cast<double>(JumpGravityMultiplier);
    if (!FMath::IsFinite(Gravity) || Gravity <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector Delta = (bUseApex ? TargetSnapshot : LandingCenter)
        - Boss->GetActorLocation();
    if (Delta.ContainsNaN()
        || (Move->bConstrainToPlane
            && !Move->ConstrainDirectionToPlane(Delta).Equals(Delta, 0.1f)))
    {
        return false;
    }

    double Height;
    double FlightTime;
    if (bUseApex)
    {
        Height = Delta.Z;
        if (Height <= 0.0)
        {
            return false;
        }
        FlightTime = FMath::Sqrt(2.0 * Height / Gravity);
    }
    else
    {
        Height = FMath::Max(static_cast<double>(PreferredJumpHeight), Delta.Z + 1.0);
        if (Height > MaxJumpHeight)
        {
            return false;
        }
        const double DescentSpeed = FMath::Sqrt(2.0 * Gravity * (Height - Delta.Z));
        const APhysicsVolume* Volume = Move->GetPhysicsVolume();
        if (!Volume || DescentSpeed >= FMath::Abs(Volume->TerminalVelocity))
        {
            return false;
        }
        FlightTime = FMath::Sqrt(2.0 * Height / Gravity)
            + FMath::Sqrt(2.0 * (Height - Delta.Z) / Gravity);
    }

    if (!FMath::IsFinite(FlightTime) || FlightTime <= UE_SMALL_NUMBER)
    {
        return false;
    }
    OutVelocity = FVector(Delta.X / FlightTime, Delta.Y / FlightTime,
        FMath::Sqrt(2.0 * Gravity * Height));
    return !OutVelocity.ContainsNaN()
        && OutVelocity.Size2D() <= MaxHorizontalSpeed;
}

void UGA_BossJumpAttack::OnSectionChanged(
    UAnimMontage* Montage, FName SectionName, bool bLooped)
{
    if (!IsActive() || bEnding || Montage != AttackMontage.Get()
        || SectionName != TEXT("JumpStart") || bStartedJump)
    {
        return;
    }
    UAnimInstance* Anim = BoundAnim.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (Instance && Instance->GetInstanceID() == MontageInstanceID)
    {
        StartJump();
    }
}

void UGA_BossJumpAttack::StartJump()
{
    if (!IsActive() || bEnding || bStartedJump)
    {
        return;
    }
    ABossMonster* Boss = GetBossOwner();
    UBossCharacterMovementComponent* Move =
        Cast<UBossCharacterMovementComponent>(Boss->GetCharacterMovement());
    FVector LaunchVelocity;
    if (!Move || !Move->IsMovingOnGround() || !BuildLaunchVelocity(LaunchVelocity))
    {
        CancelPattern();
        return;
    }

    FacingTurn.Finish();
    Move->AirControl = 0.f;
    Move->FallingLateralFriction = 0.f;
    Move->BrakingDecelerationFalling = 0.f;
    Move->MaxWalkSpeed = FMath::Max(SavedMaxSpeed, MaxHorizontalSpeed);
    Move->GravityScale *= JumpGravityMultiplier;
    Move->bJumpAttackActive = true;
    const FVector JumpGroundPoint = Move->CurrentFloor.bBlockingHit
        ? FVector(Move->CurrentFloor.HitResult.ImpactPoint)
        : Boss->GetActorLocation() - FVector(0.f, 0.f,
            Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Boss->MulticastGroundEffect(JumpStartGroundEffect.Get(), JumpGroundPoint);
    Move->SetMovementMode(MOVE_Falling);
    Move->Velocity = LaunchVelocity;
    bStartedJump = true;
    UBossAttackTrailComponent::StartForBoss(Boss, EBossTrailType::JumpAttack);
    if (JumpStartSound)
    {
        Boss->MulticastGunBurstSound(JumpStartSound.Get(), JumpGroundPoint);
    }
    SetEffect(StartEffect.Get());

    UCapsuleComponent* Capsule = Boss->GetCapsuleComponent();
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
    Capsule->SetGenerateOverlapEvents(true);
    Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    TArray<AActor*> OverlappingPlayers;
    Capsule->GetOverlappingActors(OverlappingPlayers, APlayerBase::StaticClass());
    for (AActor* Player : OverlappingPlayers)
    {
        DamagePlayer(Player);
    }
}

void UGA_BossJumpAttack::OnOverlap(
    UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    DamagePlayer(OtherActor);
}

void UGA_BossJumpAttack::DamagePlayer(AActor* OtherActor)
{
    APlayerBase* Player = Cast<APlayerBase>(OtherActor);
    ABossMonster* Boss = GetBossOwner();
    if (!IsActive() || bEnding || !bStartedJump || bLanded
        || !IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead()
        || HitPlayers.Contains(TWeakObjectPtr<AActor>(Player)))
    {
        return;
    }
    HitPlayers.Add(TWeakObjectPtr<AActor>(Player));
    FVector Direction = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero())
    {
        Direction = Boss->GetVelocity().GetSafeNormal2D();
    }
    if (Direction.IsNearlyZero())
    {
        Direction = Boss->GetActorForwardVector().GetSafeNormal2D();
    }
    if (Damage > 0.f)
    {
        UAbilitySystemComponent* SourceASC = Boss->GetAbilitySystemComponent();
        UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent();
        const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Data.Damage")), false);
        if (!IsValid(SourceASC) || !IsValid(TargetASC) || !DamageTag.IsValid()
            || !TargetASC->HasAttributeSetForAttribute(
                UCombatAttributeSet::GetIncomingDamageAttribute()))
        {
            return;
        }

        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddSourceObject(Boss);
        FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
            DamageEffectClass, GetAbilityLevel(), Context);
        if (!Spec.IsValid())
        {
            return;
        }

        Spec.Data->SetSetByCallerMagnitude(DamageTag, Damage);
        const FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Damage.Type.Melee")), false);
        if (MeleeTag.IsValid())
        {
            Spec.Data->AddDynamicAssetTag(MeleeTag);
        }

        const FActiveGameplayEffectHandle Applied =
            TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        if (!Applied.WasSuccessfullyApplied())
        {
            return;
        }
    }
    if (IsValid(Player) && !Player->IsDead())
    {
        Player->LaunchCharacter(
            Direction * KnockbackHorizontalSpeed + FVector(0.f, 0.f, KnockbackUpSpeed),
            true, true);
    }
}

void UGA_BossJumpAttack::OnLanded(const FHitResult& Hit)
{
    if (!IsActive() || bEnding || !bStartedJump || bLanded)
    {
        return;
    }
    bLanded = true;
    UBossAttackTrailComponent::StopForBoss(GetBossOwner());
    if (ABossMonster* Boss = GetBossOwner();
        IsValid(Boss) && Boss->HasAuthority() && LandingSound)
    {
        Boss->MulticastGunBurstSound(LandingSound.Get(), Hit.ImpactPoint);
    }
    SetEffect(nullptr);
    if (UCharacterMovementComponent* Move = GetBossOwner()->GetCharacterMovement())
    {
        Move->GravityScale = SavedGravityScale;
    }
    if (!PrepareLanding(Hit.ImpactPoint, MontageTask))
    {
        CancelPattern();
        return;
    }
    FinishTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
        this, &ThisClass::FinishLanding);
}

void UGA_BossJumpAttack::FinishLanding()
{
    if (IsActive() && !bEnding && bLanded && !PlayLandingMontage())
    {
        CancelPattern();
    }
}

void UGA_BossJumpAttack::OnStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Attacking)
    {
        CancelPattern();
    }
}

void UGA_BossJumpAttack::OnMontageEnded()
{
    if (!bLanded)
    {
        CancelPattern();
    }
}

void UGA_BossJumpAttack::CheckContext()
{
    ABossMonster* Boss = GetBossOwner();
    if (!IsValid(Boss) || Boss->GetBossState() != EBossState::Attacking
        || Boss->GetBossPhase() != EBossPhase::Phase3)
    {
        CancelPattern();
        return;
    }
    if (!bStartedJump && !FacingTurn.Update(GetWorld()->GetTimeSeconds()))
    {
        CancelPattern();
        return;
    }
    if (bLanded)
    {
        return;
    }
    UAnimInstance* Anim = BoundAnim.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (!Instance || Instance->GetInstanceID() != MontageInstanceID)
    {
        CancelPattern();
    }
}

void UGA_BossJumpAttack::SetEffect(UNiagaraSystem* Effect)
{
    if (IsValid(EffectComponent.Get()))
    {
        EffectComponent->DeactivateImmediate();
        EffectComponent->DestroyComponent();
    }
    EffectComponent = nullptr;
    ABossMonster* Boss = GetBossOwner();
    if (Effect && IsValid(Boss) && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            Effect, Boss->GetMesh(), TEXT("BossFlameBackSocket"),
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget, false, true, ENCPoolMethod::None, false);
    }
}

void UGA_BossJumpAttack::CancelPattern()
{
    if (IsActive() && !bEnding)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossJumpAttack::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bEnding || !IsEndAbilityValid(Handle, ActorInfo))
    {
        return;
    }
    TGuardValue<bool> EndingGuard(bEnding, true);
    UBossAttackTrailComponent::StopForBoss(GetBossOwner());
    SetEffect(nullptr);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ContextTimer);
        World->GetTimerManager().ClearTimer(FinishTimer);
    }
    if (UAnimInstance* Anim = BoundAnim.Get())
    {
        Anim->OnMontageSectionChanged.RemoveDynamic(this, &ThisClass::OnSectionChanged);
        if (bSavedSettings)
        {
            Anim->SetRootMotionMode(SavedRootMotionMode);
        }
    }
    if (MontageTask)
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
    }
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageStop(0.1f);
        ASC->ClearAnimatingAbility(this);
    }
    if (ABossMonster* Boss = GetBossOwner())
    {
        Boss->LandedDelegate.RemoveDynamic(this, &ThisClass::OnLanded);
        Boss->OnBossStateChanged.RemoveDynamic(this, &ThisClass::OnStateChanged);
        UCapsuleComponent* Capsule = Boss->GetCapsuleComponent();
        Capsule->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::OnOverlap);
        if (bSavedSettings)
        {
            Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, SavedSoftResponse);
            Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
            Capsule->SetGenerateOverlapEvents(bSavedOverlapEvents);
            if (UBossCharacterMovementComponent* Move =
                Cast<UBossCharacterMovementComponent>(Boss->GetCharacterMovement()))
            {
                Move->bJumpAttackActive = false;
                Move->GravityScale = SavedGravityScale;
                Move->AirControl = SavedAirControl;
                Move->FallingLateralFriction = SavedFriction;
                Move->BrakingDecelerationFalling = SavedBraking;
                Move->MaxAcceleration = SavedAcceleration;
                Move->MaxWalkSpeed = SavedMaxSpeed;
                if (bLanded)
                {
                    Move->StopMovementImmediately();
                }
            }
        }
        if (Boss->HasAuthority() && Boss->GetBossState() == EBossState::Attacking)
        {
            Boss->SetBossState(EBossState::Combat);
        }
    }
    bSavedSettings = false;
    BoundAnim.Reset();
    MontageTask = nullptr;
    HitPlayers.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}