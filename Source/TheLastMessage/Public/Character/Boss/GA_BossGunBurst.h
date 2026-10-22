#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "GA_BossGunBurst.generated.h"

class ABossBullet;
class APlayerBase;
class UAnimMontage;
class UNiagaraSystem;
class UAbilityTask_PlayMontageAndWait;

USTRUCT(BlueprintType)
struct FBossGunBurstConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float Damage = 7.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 ShotCount = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.01", Units = "s"))
    float ShotInterval = 1.5f;
};

UCLASS()
class THELASTMESSAGE_API UGA_BossGunBurst : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossGunBurst();

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
    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Phase")
    FBossGunBurstConfig Phase1;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Phase")
    FBossGunBurstConfig Phase2;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Bullet")
    TSubclassOf<ABossBullet> BulletClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Bullet",
        meta = (ClampMin = "1.0", Units = "cm/s"))
    float BulletSpeed = 1500.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Bullet",
        meta = (ClampMin = "0.1", Units = "s"))
    float BulletLifetime = 8.f;

    // Socket on GunMesh, not on the character's skeleton.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Bullet")
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Effects")
    TObjectPtr<UNiagaraSystem> MuzzleEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunBurst|Aim")
    FVector AimOriginOffset = FVector(0.f, 0.f, 150.f);

private:
    UFUNCTION()
    void HandleNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

    UFUNCTION()
    void HandleCompleted();

    UFUNCTION()
    void HandleCancelled();

    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);

    void UpdatePattern();
    bool IsContextValid() const;
    bool FireBullet();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    TWeakObjectPtr<UAnimInstance> BoundAnim;
    TWeakObjectPtr<APlayerBase> Target;
    FBossGunBurstConfig ActiveConfig;
    EBossPhase LockedPhase = EBossPhase::Phase1;
    FTimerHandle UpdateTimer;
    int32 MontageInstanceID = INDEX_NONE;
    int32 ShotsFired = 0;
    double NextFireStartTime = 0.0;
    double Deadline = 0.0;
    bool bAwaitingShot = false;
    bool bFireClipFinished = false;
    bool bEnding = false;
    bool bSavedRotation = false;
    bool bSavedControllerYaw = false;
    bool bSavedOrientToMovement = false;
    bool bSavedDesiredRotation = false;
};