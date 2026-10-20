#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "GA_BossCircularMissile.generated.h"

class APlayerBase;
class ABossCircularMissile;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

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
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CircularMissile|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CircularMissile|Animation")
    FName LaunchNotifyName = TEXT("LaunchStart");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CircularMissile")
    FVector MissileSpawnOffset = FVector(0.f, 0.f, 150.f);

private:
    UFUNCTION()
    void HandleLaunchNotify(FName NotifyName,
        const FBranchingPointNotifyPayload& Payload);

    UFUNCTION()
    void HandleMontageCompleted();

    UFUNCTION()
    void HandleMontageCancelled();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
    int32 MontageInstanceID = INDEX_NONE;
    bool bLaunchStarted = false;
    bool bMontageFinished = false;

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