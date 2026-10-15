#include "Character/Boss/GA_BossMelee.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"

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

    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(Target) || !Target->IsPlayerControlled() || Target->IsDead()
        || !IsValid(AnimInstance) || !AttackMontage || HitNotifyName.IsNone()
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

    FVector Facing = Target->GetActorLocation() - Boss->GetActorLocation();
    Facing.Z = 0.f;
    if (!Facing.IsNearlyZero())
    {
        Boss->SetActorRotation(Facing.Rotation());
    }

    BoundAnimInstance = AnimInstance;
    AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(
        this, &UGA_BossMelee::HandleHitNotify);

    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage, 1.f, NAME_None, true, 1.f, 0.f, true);
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
        || NotifyName != HitNotifyName || MontageInstanceID == INDEX_NONE
        || Payload.MontageInstanceID != MontageInstanceID
        || Payload.SkelMeshComponent != Boss->GetMesh())
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
    APlayerBase* Target = Cast<APlayerBase>(LockedTarget.Get());
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || Boss->GetBossState() != EBossState::Attacking
        || !IsValid(Target) || !Target->IsPlayerControlled() || Target->IsDead()
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

    UE_LOG(LogTemp, Log, TEXT("BossMelee: Hit -> %s"), *GetNameSafe(Target));
    ApplyAttackDamage(Target);
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