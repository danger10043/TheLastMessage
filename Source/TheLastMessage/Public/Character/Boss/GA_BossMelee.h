#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "Animation/AnimInstance.h"
#include "GA_BossMelee.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

UCLASS()
class THELASTMESSAGE_API UGA_BossMelee : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossMelee();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Melee")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Melee")
    FName HitNotifyName = TEXT("BossMeleeHit");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Melee",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float HitHalfAngle = 60.f;

private:
    UFUNCTION()
    void HandleHitNotify(FName NotifyName,
        const FBranchingPointNotifyPayload& Payload);

    UFUNCTION()
    void HandleMontageCompleted();

    UFUNCTION()
    void HandleMontageCancelled();

    void PerformHit();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
    TWeakObjectPtr<AActor> LockedTarget;
    int32 MontageInstanceID = INDEX_NONE;
    bool bHitProcessed = false;
    bool bEnding = false;
};