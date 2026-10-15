#include "Character/Boss/GA_BossMelee.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
    APawn* Target = IsValid(Boss) ? Cast<APawn>(Boss->GetCurrentTarget()) : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(Target) || !Target->IsPlayerControlled()
        || !FMath::IsFinite(WindupTime) || WindupTime <= 0.f
        || !FMath::IsFinite(RecoveryTime) || RecoveryTime <= 0.f
        || !FMath::IsFinite(WindupTime + RecoveryTime)
        || !FMath::IsFinite(HitHalfAngle) || HitHalfAngle < 0.f || HitHalfAngle > 180.f
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    LockedTarget = Target;
    bHitProcessed = false;
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

    if (AttackMontage)
    {
        UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
        if (!IsValid(ASC)
            || ASC->PlayMontage(this, ActivationInfo, AttackMontage, 1.f) <= 0.f)
        {
            UE_LOG(LogTemp, Warning, TEXT("BossMelee: montage failed to play."));
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    if (!IsActive())
    {
        return;
    }
    GetWorld()->GetTimerManager().SetTimer(
        HitTimer, this, &UGA_BossMelee::PerformHit, WindupTime, false);
    GetWorld()->GetTimerManager().SetTimer(
        FinishTimer, this, &UGA_BossMelee::FinishMelee,
        WindupTime + RecoveryTime, false);
    UE_LOG(LogTemp, Log, TEXT("BossMelee: Start -> %s"), *GetNameSafe(Target));
}

void UGA_BossMelee::PerformHit()
{
    if (!IsActive() || bHitProcessed)
    {
        return;
    }
    bHitProcessed = true;

    ABossMonster* Boss = GetBossOwner();
    APawn* Target = Cast<APawn>(LockedTarget.Get());
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || Boss->GetBossState() != EBossState::Attacking
        || !IsValid(Target) || !Target->IsPlayerControlled()
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

void UGA_BossMelee::FinishMelee()
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, false);
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

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitTimer);
        World->GetTimerManager().ClearTimer(FinishTimer);
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (IsValid(ASC) && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageStop(0.1f);
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