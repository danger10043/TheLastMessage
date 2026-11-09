#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossLandingAttackBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "GA_BossJumpAttack.generated.h"

class APlayerBase;
class UNiagaraSystem;
class UNiagaraComponent;
class UAnimMontage;
class UPrimitiveComponent;
class UAbilityTask_PlayMontageAndWait;
class USoundBase;

UCLASS()
class THELASTMESSAGE_API UGA_BossJumpAttack : public UGA_BossLandingAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossJumpAttack();

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
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump")
    TObjectPtr<UNiagaraSystem> ReadyEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump")
    TObjectPtr<UNiagaraSystem> StartEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump|GroundEffect")
    TObjectPtr<UNiagaraSystem> JumpStartGroundEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump|Audio")
    TObjectPtr<USoundBase> JumpStartSound;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump|Audio")
    TObjectPtr<USoundBase> LandingSound;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump|Damage", meta = (ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float JumpGravityMultiplier = 3.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float ApexHeightThreshold = 300.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float PreferredJumpHeight = 350.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float MaxJumpHeight = 600.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float MaxHorizontalSpeed = 2400.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float FloorSearchDepth = 3000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "0.0"))
    float KnockbackHorizontalSpeed = 700.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Jump", meta = (ClampMin = "0.0"))
    float KnockbackUpSpeed = 250.f;

private:
    bool BuildLaunchVelocity(FVector& OutVelocity) const;
    bool FindLandingCenter(FVector& OutCenter) const;
    void StartJump();
    void CheckContext();
    void FinishLanding();
    void DamagePlayer(AActor* OtherActor);
    void SetEffect(UNiagaraSystem* Effect);
    void CancelPattern();

    UFUNCTION()
    void OnSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

    UFUNCTION()
    void OnLanded(const FHitResult& Hit);

    UFUNCTION()
    void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnStateChanged(EBossState NewState);

    UFUNCTION()
    void OnMontageEnded();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> EffectComponent;

    TWeakObjectPtr<UAnimInstance> BoundAnim;
    TSet<TWeakObjectPtr<AActor>> HitPlayers;
    FVector TargetSnapshot = FVector::ZeroVector;
    FVector LandingCenter = FVector::ZeroVector;
    FTimerHandle ContextTimer;
    FTimerHandle FinishTimer;
    int32 MontageInstanceID = INDEX_NONE;
    ECollisionResponse SavedSoftResponse = ECR_Block;
    ECollisionResponse SavedPawnResponse = ECR_Block;
    ERootMotionMode::Type SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
    float SavedAirControl = 0.f;
    float SavedFriction = 0.f;
    float SavedBraking = 0.f;
    float SavedAcceleration = 0.f;
    float SavedMaxSpeed = 0.f;
    float SavedGravityScale = 1.f;
    float TargetHalfHeight = 0.f;
    bool bSavedOverlapEvents = false;
    bool bUseApex = false;
    bool bStartedJump = false;
    bool bLanded = false;
    bool bSavedSettings = false;
    bool bEnding = false;
};