#include "Character/Boss/GA_BossMelee.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttackTrailComponent.h"
#include "Character/Player/PlayerBase.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "Sound/SoundBase.h"

UGA_BossMelee::UGA_BossMelee()
{
    AttackType = EBossAttackType::Melee;
}

void UGA_BossMelee::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;

    UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;

    FName StartSection = NAME_None;
    if (IsValid(Boss))
    {
        switch (Boss->GetBossPhase())
        {
        case EBossPhase::Phase1:
        case EBossPhase::Phase2:
            StartSection = TEXT("Phase12");
            break;

        case EBossPhase::Phase3:
            StartSection = TEXT("Phase3");
            break;

        default:
            break;
        }
    }

    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(Target) || !Target->IsPlayerControlled() || Target->IsDead()
        || !IsValid(AnimInstance) || !AttackMontage || HitNotifyName.IsNone()
        || StartSection.IsNone()
        || AttackMontage->GetSectionIndex(StartSection) == INDEX_NONE
        || !FMath::IsFinite(HitHalfAngle) || HitHalfAngle < 0.f || HitHalfAngle > 180.f
        || !Boss->IsDistanceInAttackRange(AttackType, Boss->GetDistanceTo(Target))
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    LockedTarget = Target;
    bHitProcessed = false;
    MontageInstanceID = INDEX_NONE;
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive())
    {
        return;
    }

    if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
    {
        AI->StopMovement();
    }
    if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    BoundAnimInstance = AnimInstance;
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(
        this, &UGA_BossMelee::HandleHitNotify);

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackMontage, 1.f, StartSection, true, 1.f, 0.f, true);
    if (!IsValid(MontageTask.Get()))
    {
        HandleMontageCancelled();
        return;
    }

    MontageTask->OnCompleted.AddDynamic(this, &UGA_BossMelee::HandleMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossMelee::HandleMontageCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_BossMelee::HandleMontageCancelled);
    MontageTask->ReadyForActivation();

    if (!IsActive())
    {
        return;
    }
    FAnimMontageInstance* Instance =
        AnimInstance->GetActiveInstanceForMontage(AttackMontage);
    if (!Instance)
    {
        HandleMontageCancelled();
        return;
    }
    MontageInstanceID = Instance->GetInstanceID();
    UE_LOG(LogTemp, Log, TEXT("BossMelee: Start -> %s"), *GetNameSafe(Target));
}

void UGA_BossMelee::HandleHitNotify(
    FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
    ABossMonster* Boss = GetBossOwner();
    if (!IsActive() || bEnding || !IsValid(Boss) || !Boss->HasAuthority()
        || MontageInstanceID == INDEX_NONE
        || Payload.MontageInstanceID != MontageInstanceID
        || Payload.SkelMeshComponent != Boss->GetMesh())
    {
        return;
    }

    if (NotifyName == TEXT("TrailStart"))
    {
        UAnimInstance* Anim = BoundAnimInstance.Get();
        const FName Section = Anim
            ? Anim->Montage_GetCurrentSection(AttackMontage)
            : NAME_None;

        if (Section == TEXT("Phase12"))
        {
            UBossAttackTrailComponent::StartForBoss(
                Boss, EBossTrailType::MeleePhase12);
        }
        else if (Section == TEXT("Phase3"))
        {
            UBossAttackTrailComponent::StartForBoss(
                Boss, EBossTrailType::MeleePhase3);
        }
        return;
    }

    if (NotifyName == TEXT("TrailEnd"))
    {
        UBossAttackTrailComponent::StopForBoss(Boss);
        return;
    }

    if (NotifyName != HitNotifyName)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("BossMelee: Notify -> %s"), *NotifyName.ToString());
    PerformHit();
}

void UGA_BossMelee::PerformHit()
{
    if (!IsActive() || bHitProcessed)
    {
        return;
    }
    bHitProcessed = true;

    ABossMonster* Boss = GetBossOwner();
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || Boss->GetBossState() != EBossState::Attacking)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    UAnimInstance* Anim = BoundAnimInstance.Get();
    const FName Section = Anim
        ? Anim->Montage_GetCurrentSection(AttackMontage)
        : NAME_None;
    const bool bPhase12Attack = Section == TEXT("Phase12");

    USoundBase* AttackSound = nullptr;
    if (bPhase12Attack)
    {
        AttackSound = Phase12AttackSound.Get();
    }
    else if (Section == TEXT("Phase3"))
    {
        AttackSound = Phase3AttackSound.Get();
    }

    // 공격음은 명중 여부와 관계없이 타격 노티파이 시점에 재생합니다.
    if (IsValid(AttackSound))
    {
        Boss->MulticastGunBurstSound(AttackSound, Boss->GetActorLocation());
    }

    APlayerBase* Target = Cast<APlayerBase>(LockedTarget.Get());
    if (!IsValid(Target) || !Target->IsPlayerControlled() || Target->IsDead()
        || Boss->GetCurrentTarget() != Target)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    const FVector Direction =
        (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal();
    const float FacingDot = FVector::DotProduct(
        Boss->GetActorForwardVector(), Direction);
    AAIController* AI = Cast<AAIController>(Boss->GetController());

    if (!Boss->IsDistanceInAttackRange(AttackType, Boss->GetDistanceTo(Target))
        || FacingDot < FMath::Cos(FMath::DegreesToRadians(HitHalfAngle))
        || !IsValid(AI) || !AI->LineOfSightTo(Target))
    {
        UE_LOG(LogTemp, Log, TEXT("BossMelee: Miss"));
        return;
    }

    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    const FGameplayAttribute HealthAttribute =
        UCombatAttributeSet::GetHealthAttribute();

    if (!IsValid(TargetASC)
        || !TargetASC->HasAttributeSetForAttribute(HealthAttribute))
    {
        return;
    }

    const float PreviousHealth = TargetASC->GetNumericAttribute(HealthAttribute);
    const FVector HitLocation = Target->GetActorLocation();
    USoundBase* PlayerHitSound =
        bPhase12Attack ? Phase12PlayerHitSound.Get() : nullptr;
    const float Strength = FMath::IsFinite(KnockbackStrength)
        ? FMath::Max(0.f, KnockbackStrength) : 0.f;

    UE_LOG(LogTemp, Log, TEXT("BossMelee: Hit -> %s"), *GetNameSafe(Target));
    ApplyAttackDamage(Target);

    if (!IsValid(Target) || !IsValid(TargetASC))
    {
        return;
    }

    const float CurrentHealth = TargetASC->GetNumericAttribute(HealthAttribute);
    if (!FMath::IsFinite(PreviousHealth) || !FMath::IsFinite(CurrentHealth)
        || CurrentHealth >= PreviousHealth)
    {
        return;
    }

    // 치명타격으로 사망한 경우에도 Phase1·2 피격음은 재생합니다.
    if (IsValid(Boss) && IsValid(PlayerHitSound))
    {
        Boss->MulticastGunBurstSound(PlayerHitSound, HitLocation);
    }

    // 피해 적용 과정의 피격 반응이 이동을 멈춘 뒤 넉백을 적용합니다.
    if (!Target->IsDead() && Strength > 0.f && !Direction.IsNearlyZero())
    {
        Target->LaunchCharacter(Direction * Strength, true, true);
    }
}

void UGA_BossMelee::HandleMontageCompleted()
{
    if (!bHitProcessed)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMelee: completed without hit notify. Check Montage Notify settings."));
    }
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, false);
}

void UGA_BossMelee::HandleMontageCancelled()
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, true);
}

void UGA_BossMelee::EndAbility(
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

    if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
    {
        AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(
            this, &UGA_BossMelee::HandleHitNotify);
    }
    BoundAnimInstance.Reset();
    MontageInstanceID = INDEX_NONE;

    if (IsValid(MontageTask.Get()))
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
    }
    MontageTask = nullptr;

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (IsValid(ASC) && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageStop(0.1f);
        ASC->ClearAnimatingAbility(this);
    }
    LockedTarget.Reset();

    if (ABossMonster* Boss = GetBossOwner())
    {
        if (Boss->HasAuthority() && Boss->GetBossState() == EBossState::Attacking)
        {
            Boss->SetBossState(EBossState::Combat);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("BossMelee: End (cancelled=%d)"), bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo,
        bReplicateEndAbility, bWasCancelled);
}