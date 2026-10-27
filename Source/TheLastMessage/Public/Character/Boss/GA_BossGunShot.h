#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "TimerManager.h"
#include "GA_BossGunShot.generated.h"

class ABossBullet;
class APlayerBase;
class UAnimInstance;
class UAnimMontage;
class UNiagaraSystem;
class UAbilityTask_PlayMontageAndWait;

USTRUCT(BlueprintType)
struct FBossGunShotConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
    float Damage = 5.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 BulletCount = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", Units = "deg"))
    float AnglePerBullet = 15.f;
};

UCLASS()
class THELASTMESSAGE_API UGA_BossGunShot : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossGunShot();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Phase")
    FBossGunShotConfig Phase1;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Phase")
    FBossGunShotConfig Phase2;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Bullet")
    TSubclassOf<ABossBullet> BulletClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Bullet", meta = (ClampMin = "1.0", Units = "cm/s"))
    float BulletSpeed = 1500.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Bullet", meta = (ClampMin = "0.1", Units = "s"))
    float BulletLifetime = 8.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Bullet")
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Effects")
    TObjectPtr<UNiagaraSystem> MuzzleEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|GunShot|Aim")
    FVector AimOriginOffset = FVector(0.f, 0.f, 150.f);

private:
    UFUNCTION()
    void HandleSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

    UFUNCTION()
    void HandleCompleted();

    UFUNCTION()
    void HandleCancelled();

    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);

    void UpdatePattern();

    bool IsContextValid() const;

    bool FireFan();

    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    TWeakObjectPtr<UAnimInstance> BoundAnim;

    TWeakObjectPtr<APlayerBase> Target;

    FBossGunShotConfig ActiveConfig;

    EBossPhase LockedPhase = EBossPhase::Phase1;

    FTimerHandle UpdateTimer;

    double Deadline = 0.0;

    int32 MontageInstanceID = INDEX_NONE;

    bool bFired = false;

    bool bEnding = false;

    bool bSavedRotation = false;

    bool bSavedControllerYaw = false;

    bool bSavedOrientToMovement = false;

    bool bSavedDesiredRotation = false;
};