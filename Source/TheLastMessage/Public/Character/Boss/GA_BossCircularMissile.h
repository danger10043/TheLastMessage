#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "TimerManager.h"
#include "GA_BossCircularMissile.generated.h"

class APlayerBase;
class ABossCircularMissile;

UCLASS()
class THELASTMESSAGE_API UGA_BossCircularMissile : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossCircularMissile();

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
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CircularMissile")
    FVector MissileSpawnOffset = FVector(0.f, 0.f, 150.f);

private:
    void SpawnNextMissile();
    void CheckMissilesFinished();

    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);

    UPROPERTY(Transient)
    FBossMissileConfig ActiveConfig;

    TWeakObjectPtr<APlayerBase> LockedTarget;

    TArray<TWeakObjectPtr<ABossCircularMissile>> SpawnedMissiles;

    FTimerHandle SpawnTimer;

    FTimerHandle FinishTimer;

    FVector TargetA = FVector::ZeroVector;

    FVector TargetB = FVector::ZeroVector;

    EBossPhase LockedPhase = EBossPhase::Phase1;

    int32 SpawnedCount = 0;

    bool bEnding = false;
};