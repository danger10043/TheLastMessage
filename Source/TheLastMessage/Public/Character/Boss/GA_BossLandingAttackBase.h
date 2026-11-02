#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "Character/Boss/BossFacingTurn.h"
#include "GA_BossLandingAttackBase.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class UCameraShakeBase;
class UAbilityTask_PlayMontageAndWait;

UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UGA_BossLandingAttackBase : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Landing")
    TObjectPtr<UAnimMontage> LandingMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Landing")
    TObjectPtr<UNiagaraSystem> LandingEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Landing")
    TSubclassOf<UCameraShakeBase> LandingCameraShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Landing", meta = (ClampMin = "0.0"))
    float LandingCameraShakeScale = 1.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Landing",
        meta = (ClampMin = "0.0", Units = "cm"))
    float LandingKnockbackRadius = 500.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Landing",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float LandingKnockbackSpeed = 1000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Facing")
    TObjectPtr<class UCurveFloat> FacingTurnSpeedCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Facing",
        meta = (ClampMin = "0.01", Units = "s"))
    float FacingTurnDuration = 0.35f;

    FBossFacingTurn FacingTurn;

    bool PrepareLanding(const FVector& GroundPoint,
        TObjectPtr<UAbilityTask_PlayMontageAndWait>& PreviousTask);
    bool PlayLandingMontage();

private:
    bool IsLandingConfigValid() const;

    UFUNCTION()
    void HandleLandingCompleted();

    UFUNCTION()
    void HandleLandingCancelled();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> LandingMontageTask;

    bool bLandingPrepared = false;
    bool bLandingCleanup = false;
};