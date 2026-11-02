#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "GA_BossRockThrow.generated.h"

class ABossRockProjectile;
class APlayerBase;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

UCLASS()
class THELASTMESSAGE_API UGA_BossRockThrow : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossRockThrow();

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

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
    UPROPERTY(EditDefaultsOnly, Category = "Boss|RockThrow")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|RockThrow")
    TSubclassOf<ABossRockProjectile> RockClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|RockThrow", meta = (ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|RockThrow",
        meta = (ClampMin = "1.0", Units = "cm/s"))
    float RockSpeed = 1800.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|RockThrow",
        meta = (ClampMin = "0.1", Units = "s"))
    float RockLifetime = 10.f;

private:
    bool IsContextValid() const;
    bool ThrowRock();
    void CheckContext();

    UFUNCTION()
    void HandleNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

    UFUNCTION()
    void HandleCompleted();

    UFUNCTION()
    void HandleCancelled();

    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    TWeakObjectPtr<UAnimInstance> BoundAnim;
    TWeakObjectPtr<APlayerBase> Target;
    FTimerHandle ContextTimer;
    double Deadline = 0.0;
    int32 MontageInstanceID = INDEX_NONE;
    bool bThrown = false;
    bool bOwnsBossState = false;
    bool bEnding = false;
};