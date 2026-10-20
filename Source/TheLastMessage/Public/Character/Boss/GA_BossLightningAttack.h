#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "TimerManager.h"
#include "GA_BossLightningAttack.generated.h"

class ABossLightning;
class APlayerBase;
class UAnimInstance;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UNiagaraSystem;
class UNiagaraComponent;

USTRUCT(BlueprintType)
struct FBossLightningPhaseConfig
{
    GENERATED_BODY()

    FBossLightningPhaseConfig() = default;
    FBossLightningPhaseConfig(float InDamage, float InRadius, float InWarning,
        float InDuration, float InInterval)
        : Damage(InDamage), Radius(InRadius), WarningTime(InWarning),
        StormDuration(InDuration), SpawnInterval(InInterval) {}

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0"))
    float Damage = 5.f;

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "1.0"))
    float Radius = 100.f;

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.0"))
    float WarningTime = 0.7f;

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
    float StormDuration = 4.f;

    UPROPERTY(EditDefaultsOnly, meta = (ClampMin = "0.01"))
    float SpawnInterval = 0.5f;
};

UCLASS()
class THELASTMESSAGE_API UGA_BossLightningAttack : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossLightningAttack();

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
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning")
    TSubclassOf<ABossLightning> LightningClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning|Effects")
    TObjectPtr<UNiagaraSystem> StormEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning")
    FBossLightningPhaseConfig Phase1Config;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning")
    FBossLightningPhaseConfig Phase2Config =
        FBossLightningPhaseConfig(7.f, 120.f, 0.75f, 5.f, 0.4f);

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning")
    FBossLightningPhaseConfig Phase3Config =
        FBossLightningPhaseConfig(10.f, 140.f, 0.8f, 6.f, 0.3f);

private:
    void StopStormEffect();

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> StormEffectComponent;

    UFUNCTION()
    void HandleSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

    UFUNCTION()
    void HandleMontageCompleted();

    UFUNCTION()
    void HandleMontageCancelled();

    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);

    bool IsPatternContextValid() const;
    void CheckPatternContext();
    void StartStorm();
    void SpawnLightning();
    void StopStorm();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY(Transient)
    FBossLightningPhaseConfig ActiveConfig;

    TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
    TWeakObjectPtr<APlayerBase> LockedTarget;
    EBossPhase LockedPhase = EBossPhase::Phase1;
    FTimerHandle SpawnTimer;
    FTimerHandle StormTimer;
    FTimerHandle ContextTimer;
    double StormEndTime = 0.0;
    int32 MontageInstanceID = INDEX_NONE;
    bool bStormStarted = false;
    bool bStormActive = false;
    bool bEndRequested = false;
    bool bEnding = false;
};