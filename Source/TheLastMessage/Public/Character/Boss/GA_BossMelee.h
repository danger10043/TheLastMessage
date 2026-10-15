#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "GA_BossMelee.generated.h"

class UAnimMontage;

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

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Melee", meta = (ClampMin = "0.01"))
    float WindupTime = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Melee", meta = (ClampMin = "0.01"))
    float RecoveryTime = 0.45f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Melee",
        meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float HitHalfAngle = 60.f;

private:
    void PerformHit();
    void FinishMelee();

    TWeakObjectPtr<AActor> LockedTarget;
    FTimerHandle HitTimer;
    FTimerHandle FinishTimer;
    bool bHitProcessed = false;
    bool bEnding = false;
};