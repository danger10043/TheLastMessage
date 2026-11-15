#include "Character/Boss/GA_BossLandingAttackBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/GameInstance.h"
#include "UI/GameVisualSettingsSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraSystem.h"

bool UGA_BossLandingAttackBase::IsLandingConfigValid() const
{
    return IsValid(LandingMontage.Get()) && IsValid(LandingEffect.Get())
        && !LandingMontage->HasRootMotion()
        && FMath::IsFinite(LandingMontage->GetPlayLength())
        && LandingMontage->GetPlayLength() > 0.f
        && FMath::IsFinite(LandingMontage->RateScale)
        && LandingMontage->RateScale > 0.f
        && FMath::IsFinite(LandingCameraShakeScale) && LandingCameraShakeScale >= 0.f
        && (LandingCameraShakeScale == 0.f || LandingCameraShakeClass)
        && FMath::IsFinite(LandingKnockbackRadius) && LandingKnockbackRadius >= 0.f
        && FMath::IsFinite(LandingKnockbackSpeed) && LandingKnockbackSpeed >= 0.f;
}

bool UGA_BossLandingAttackBase::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    return IsLandingConfigValid()
        && Super::CanActivateAbility(
            Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UGA_BossLandingAttackBase::PrepareLanding(
    const FVector& GroundPoint,
    TObjectPtr<UAbilityTask_PlayMontageAndWait>& PreviousTask)
{
    ABossMonster* Boss = GetBossOwner();
    if (!IsActive() || bLandingPrepared || bLandingCleanup
        || !IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || GroundPoint.ContainsNaN() || !IsLandingConfigValid())
    {
        return false;
    }
    bLandingPrepared = true;

    if (PreviousTask)
    {
        PreviousTask->OnCompleted.RemoveAll(this);
        PreviousTask->OnInterrupted.RemoveAll(this);
        PreviousTask->OnCancelled.RemoveAll(this);
        PreviousTask->EndTask();
        PreviousTask = nullptr;
    }
    if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }
    Boss->MulticastGroundEffect(LandingEffect.Get(), GroundPoint);

    for (TActorIterator<APlayerBase> It(GetWorld()); It; ++It)
    {
        APlayerBase* Player = *It;
        if (!IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead())
        {
            continue;
        }

        if (LandingCameraShakeClass && LandingCameraShakeScale > 0.f)
        {
            if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
            {
                const auto* Visual = PC->IsLocalController()
                    ? UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetWorld()->GetGameInstance()) : nullptr;
                const float Scale = LandingCameraShakeScale * (Visual ? Visual->ShakeScale : 1.f);
                if (Scale > 0.f) PC->ClientStartCameraShake(LandingCameraShakeClass, Scale);
            }
        }

        const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
        if (!Capsule || LandingKnockbackRadius <= 0.f || LandingKnockbackSpeed <= 0.f)
        {
            continue;
        }
        const FVector Center = Capsule->GetComponentLocation();
        const double CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        const double SegmentHalfHeight = FMath::Max(
            0.0, Capsule->GetScaledCapsuleHalfHeight() - CapsuleRadius);
        const FVector ClosestAxisPoint(Center.X, Center.Y,
            FMath::Clamp(GroundPoint.Z,
                Center.Z - SegmentHalfHeight, Center.Z + SegmentHalfHeight));
        const double Reach = LandingKnockbackRadius + CapsuleRadius;
        if (FVector::DistSquared(ClosestAxisPoint, GroundPoint) > Reach * Reach)
        {
            continue;
        }

        const float Sign = Player->GetActorLocation().X < Boss->GetActorLocation().X
            ? -1.f : 1.f;
        Player->LaunchCharacter(
            FVector(Sign * LandingKnockbackSpeed, 0.f, 0.f), true, false);
    }
    return IsActive();
}

bool UGA_BossLandingAttackBase::PlayLandingMontage()
{
    if (!IsActive() || !bLandingPrepared || bLandingCleanup
        || LandingMontageTask || !IsLandingConfigValid())
    {
        return false;
    }
    LandingMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, LandingMontage.Get(), 1.f, NAME_None, true, 1.f, 0.f, true);
    if (!LandingMontageTask)
    {
        return false;
    }
    LandingMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleLandingCompleted);
    LandingMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleLandingCancelled);
    LandingMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleLandingCancelled);
    LandingMontageTask->ReadyForActivation();
    if (!IsActive())
    {
        return false;
    }

    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    UAnimInstance* Anim = Info ? Info->GetAnimInstance() : nullptr;
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(LandingMontage.Get()) : nullptr;
    if (!Instance)
    {
        return false;
    }
    Instance->bEnableAutoBlendOut = true;
    return true;
}

void UGA_BossLandingAttackBase::HandleLandingCompleted()
{
    if (IsActive() && bLandingPrepared && !bLandingCleanup)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, false);
    }
}

void UGA_BossLandingAttackBase::HandleLandingCancelled()
{
    if (IsActive() && !bLandingCleanup)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossLandingAttackBase::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bLandingCleanup || !IsEndAbilityValid(Handle, ActorInfo))
    {
        return;
    }
    TGuardValue<bool> CleanupGuard(bLandingCleanup, true);
    FacingTurn.Restore();
    if (LandingMontageTask)
    {
        LandingMontageTask->OnCompleted.RemoveAll(this);
        LandingMontageTask->OnInterrupted.RemoveAll(this);
        LandingMontageTask->OnCancelled.RemoveAll(this);
        LandingMontageTask->EndTask();
        LandingMontageTask = nullptr;
    }
    bLandingPrepared = false;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
