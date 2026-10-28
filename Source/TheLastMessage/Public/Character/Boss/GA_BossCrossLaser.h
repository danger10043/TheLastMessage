#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/GA_BossAttackBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "GA_BossCrossLaser.generated.h"

class UAnimMontage;
class UCurveFloat;
class UNiagaraSystem;
class UNiagaraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UAbilityTask_PlayMontageAndWait;

USTRUCT(BlueprintType)
struct FBossCrossLaserPhaseConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    float Damage = 30.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1"))
    int32 BeamCount = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.01"))
    float Duration = 6.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
    float MaxDegreesPerSecond = 120.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bFireCircularMissiles = false;
};

UCLASS()
class THELASTMESSAGE_API UGA_BossCrossLaser : public UGA_BossAttackBase
{
    GENERATED_BODY()

public:
    UGA_BossCrossLaser();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Phase")
    FBossCrossLaserPhaseConfig Phase1;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Phase")
    FBossCrossLaserPhaseConfig Phase2;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Phase")
    FBossCrossLaserPhaseConfig Phase3;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Animation")
    TObjectPtr<UAnimMontage> JumpMontage;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|VFX")
    TObjectPtr<UNiagaraSystem> ReadyFlame;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|VFX")
    TObjectPtr<UNiagaraSystem> FlyFlame;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|VFX")
    FName FlameSocket = TEXT("BossFlameBackSocket");

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|VFX")
    TObjectPtr<UNiagaraSystem> FiringEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Impact")
    TObjectPtr<UNiagaraSystem> LaserImpactEffect;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Impact", meta = (ClampMin = "0.01"))
    float LaserImpactInterval = 0.1f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "1"))
    float ExitDistance = 2500.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0.01"))
    float ExitDuration = 0.6f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0"))
    float JumpHeight = 300.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0"))
    float ExitDropHeight = 1500.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0"))
    float OffMapDelay = 1.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "1"))
    float RiseDepth = 5000.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0.01"))
    float RiseDuration = 3.f;
    // X: normalized time, Y: non-negative relative speed. Integrated and normalized.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Movement")
    TObjectPtr<UCurveFloat> RiseSpeedCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Animation")
    TObjectPtr<UAnimMontage> ChargeMontage;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser", meta = (ClampMin = "1"))
    float TraceLength = 20000.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser", meta = (ClampMin = "1"))
    float BeamWidth = 30.f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser", meta = (ClampMin = "0.01"))
    float DamageInterval = 0.5f;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser")
    TEnumAsByte<ECollisionChannel> LaserTraceChannel = ECC_GameTraceChannel6;
    // X: normalized firing time, Y: speed multiplier clamped to [0, 1].
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UCurveFloat> RotationSpeedCurve;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> WarningMaterial;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> LaserMaterial;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> TriangleMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Fade")
    TObjectPtr<UCurveFloat> WarningOpacityCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Fade")
    TObjectPtr<UCurveFloat> FiringOpacityCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Fade", meta = (ClampMin = "0.01"))
    float FiringFadeInDuration = 0.35f;

    // A finite missile batch begins when live lasers start. Independent of the other GA.
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Missiles")
    FBossMissileConfig MissileConfig;
    UPROPERTY(EditDefaultsOnly, Category = "Boss|CrossLaser|Missiles")
    FVector MissileSpawnOffset = FVector(0.f, 0.f, 150.f);

private:
    enum class EStage : uint8 { WaitingNotify, Exiting, OffMap, Rising, Warning, Firing };
    EStage Stage = EStage::WaitingNotify;
    EBossPhase LockedPhase = EBossPhase::Phase1;
    FBossCrossLaserPhaseConfig Config;
    FTransform SavedTransform;
    FVector RiseStart, RiseEnd, PairTargetA, PairTargetB;
    uint8 SavedMovementMode = 0, SavedCustomMode = 0;
    bool bSavedPlaneConstraint = false, bSavedCollision = true;
    bool bOwnsMovement = false, bEnding = false;
    bool bSavedUseControllerYaw = false;
    double StageStart = 0, PreviousTime = 0, NextMissileTime = 0;
    float Angle = 0.f;
    int32 MontageInstanceID = INDEX_NONE, MissileCount = 0;
    TArray<float> RiseIntegral;
    TMap<TWeakObjectPtr<AActor>, double> NextDamageTime;
    TArray<double> NextImpactTimes;
    TArray<TWeakObjectPtr<class ABossCircularMissile>> Missiles;
    TWeakObjectPtr<AActor> LockedTarget;
    TWeakObjectPtr<UAnimInstance> BoundAnim;
    FTimerHandle UpdateTimer;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> Flame;
    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> FiringEffectComponent;
    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> Geometry;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> WarningLaserMID;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FiringLaserMID;
    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UFUNCTION()
    void OnJumpNotify(FName Name, const FBranchingPointNotifyPayload& Payload);
    UFUNCTION()
    void OnMontageCompleted();
    UFUNCTION()
    void OnMontageCancelled();
    UFUNCTION()
    void OnStateChanged(EBossState NewState);
    void UpdatePattern();
    void SwitchFlame(UNiagaraSystem* System);
    bool BuildRiseIntegral();
    float RiseAlpha(float Time) const;
    void DrawLasers(bool bDamage, double Now);
    void WriteSection(int32 Index, const TArray<FVector>& Vertices,
        UMaterialInterface* Material);
    void DamagePlayer(AActor* Target, double Now);
    void SpawnMissile(double Now);
    void Finish(bool bCancelled);
};