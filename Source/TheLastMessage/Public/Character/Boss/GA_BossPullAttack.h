#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "Tickable.h"
#include "GA_BossPullAttack.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UNiagaraSystem;
class UNiagaraComponent;
class APlayerBase;
class USoundBase;

UCLASS()
class THELASTMESSAGE_API UGA_BossPullAttack
    : public UGA_BossAttackBase, public FTickableGameObject
{
    GENERATED_BODY()

public:
    UGA_BossPullAttack();

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

    virtual void BeginDestroy() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;
    virtual UWorld* GetTickableGameObjectWorld() const override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Animation")
    FName ReadySection = TEXT("PullReady");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Animation")
    FName LoopSection = TEXT("PullLoop");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Animation")
    FName EndSection = TEXT("PullEnd");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Audio")
    TObjectPtr<USoundBase> PullLoopEntrySound;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull", meta = (ClampMin = "0.01", Units = "s"))
    float PullDuration = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull", meta = (ClampMin = "1.0", Units = "cm"))
    float PullRange = 1200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull", meta = (ClampMin = "1.0", Units = "cm"))
    float DamageRange = 350.f;

    // 질량과 무관한 수평 가속도(cm/s²).
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull", meta = (ClampMin = "0.0"))
    float PullStrength = 1800.f;

    // 플레이어 중심과 범위 중심 사이에 허용하는 높이 차이.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull", meta = (ClampMin = "1.0", Units = "cm"))
    float RangeHalfHeight = 250.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Damage", meta = (ClampMin = "0.0"))
    float DamagePerTick = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Damage", meta = (ClampMin = "0.01", Units = "s"))
    float DamageInterval = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects")
    TObjectPtr<UNiagaraSystem> RangeEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects")
    TObjectPtr<UNiagaraSystem> DamageAreaEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects")
    FName ScaleParameter = TEXT("User.Scale");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects")
    FName AlphaParameter = TEXT("User.Alpha");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects", meta = (ClampMin = "0.01", Units = "s"))
    float RangeFadeInDuration = 1.f;


    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects", meta = (ClampMin = "0.01", Units = "cm"))
    float EffectBaseRadius = 100.f;

    // 보스 캡슐 중심 기준. 판정 중심과 두 효과에 함께 사용한다.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Pull|Effects")
    FVector CenterOffset = FVector(0.f, 0.f, -250.f);

private:
    bool IsContextValid() const;
    bool IsConfigValid() const;
    void StartPull();
    void StopPull();
    void ApplyPullDamage(APlayerBase* Player);
    void DestroyEffects();
    void SpawnEffect(UNiagaraSystem* System, float Radius,
        TObjectPtr<UNiagaraComponent>& OutComponent, float InitialAlpha = 1.f);

    UFUNCTION()
    void HandleSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

    UFUNCTION()
    void HandleCompleted();

    UFUNCTION()
    void HandleCancelled();

    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> RangeComponent;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> DamageComponent;

    TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
    int32 MontageInstanceID = INDEX_NONE;
    double RangeEffectStartTime = 0.0;
    double PullEndTime = 0.0;
    double NextDamageTime = 0.0;
    bool bPullStarted = false;
    bool bPullActive = false;
    bool bEndRequested = false;
    bool bEnding = false;
};