#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/Boss/BossTypes.h"
#include "Character/Boss/GA_BossCrossLaser.h"
#include "Character/Boss/GA_BossGunBurst.h"
#include "Character/Boss/GA_BossGunShot.h"
#include "Character/Boss/GA_BossLightningAttack.h"
#include "BossCombatData.generated.h"

class ABossCircularMissile;
class ABossRockProjectile;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FBossLandingAttackBaseSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|Landing")
    TObjectPtr<UAnimMontage> LandingMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Landing")
    TObjectPtr<UNiagaraSystem> LandingEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Landing")
    TSubclassOf<UCameraShakeBase> LandingCameraShakeClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Landing", meta = (ClampMin = "0.0"))
    float LandingCameraShakeScale = 1.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Landing",
        meta = (ClampMin = "0.0", Units = "cm"))
    float LandingKnockbackRadius = 500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Landing",
        meta = (ClampMin = "0.0", Units = "cm/s"))
    float LandingKnockbackSpeed = 1000.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Facing")
    TObjectPtr<class UCurveFloat> FacingTurnSpeedCurve;

    UPROPERTY(EditAnywhere, Category = "Boss|Facing",
        meta = (ClampMin = "0.01", Units = "s"))
    float FacingTurnDuration = 0.35f;
};

USTRUCT(BlueprintType)
struct FBossCircularMissileSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Phase")
    FBossMissileConfig Phase1MissileConfig;

    UPROPERTY(EditAnywhere, Category = "Phase")
    FBossMissileConfig Phase2MissileConfig;

    UPROPERTY(EditAnywhere, Category = "Phase")
    FBossMissileConfig Phase3MissileConfig;

    UPROPERTY(EditAnywhere, Category = "Boss|CircularMissile|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|CircularMissile|Animation")
    FName LaunchNotifyName = TEXT("LaunchStart");

    UPROPERTY(EditAnywhere, Category = "Boss|CircularMissile|Audio")
    TObjectPtr<USoundBase> LaunchSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CircularMissile")
    FVector MissileSpawnOffset = FVector(0.f, 0.f, 150.f);
};

USTRUCT(BlueprintType)
struct FBossLightningAttackSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning")
    TSubclassOf<ABossLightning> LightningClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning|Effects")
    TObjectPtr<UNiagaraSystem> StormEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning|Audio")
    TObjectPtr<USoundBase> StormLoopSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning|Audio")
    TObjectPtr<USoundBase> StormEndSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning")
    FBossLightningPhaseConfig Phase1Config;

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning")
    FBossLightningPhaseConfig Phase2Config =
        FBossLightningPhaseConfig(7.f, 120.f, 0.75f, 5.f, 0.4f);

    UPROPERTY(EditAnywhere, Category = "Boss|Lightning")
    FBossLightningPhaseConfig Phase3Config =
        FBossLightningPhaseConfig(10.f, 140.f, 0.8f, 6.f, 0.3f);
};

USTRUCT(BlueprintType)
struct FBossGunBurstSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Phase")
    FBossGunBurstConfig Phase1;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Phase")
    FBossGunBurstConfig Phase2;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Bullet")
    TSubclassOf<ABossBullet> BulletClass;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Bullet",
        meta = (ClampMin = "1.0", Units = "cm/s"))
    float BulletSpeed = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Bullet",
        meta = (ClampMin = "0.1", Units = "s"))
    float BulletLifetime = 8.f;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Bullet")
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Effects")
    TObjectPtr<UNiagaraSystem> MuzzleEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Audio")
    TObjectPtr<USoundBase> ShotSound;

    UPROPERTY(EditAnywhere, Category = "Boss|GunBurst|Aim")
    FVector AimOriginOffset = FVector(0.f, 0.f, 150.f);
};

USTRUCT(BlueprintType)
struct FBossGunShotSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Phase")
    FBossGunShotConfig Phase1;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Phase")
    FBossGunShotConfig Phase2;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Bullet")
    TSubclassOf<ABossBullet> BulletClass;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Bullet", meta = (ClampMin = "1.0", Units = "cm/s"))
    float BulletSpeed = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Bullet", meta = (ClampMin = "0.1", Units = "s"))
    float BulletLifetime = 8.f;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Bullet")
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Effects")
    TObjectPtr<UNiagaraSystem> MuzzleEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Audio")
    TObjectPtr<USoundBase> ShotSound;

    UPROPERTY(EditAnywhere, Category = "Boss|GunShot|Aim")
    FVector AimOriginOffset = FVector(0.f, 0.f, 150.f);
};

USTRUCT(BlueprintType)
struct FBossCrossLaserSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Phase")
    FBossCrossLaserPhaseConfig Phase1;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Phase")
    FBossCrossLaserPhaseConfig Phase2;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Phase")
    FBossCrossLaserPhaseConfig Phase3;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Animation")
    TObjectPtr<UAnimMontage> JumpMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|VFX")
    TObjectPtr<UNiagaraSystem> ReadyFlame;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|VFX")
    TObjectPtr<UNiagaraSystem> FlyFlame;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|VFX")
    FName FlameSocket = TEXT("BossFlameBackSocket");

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|VFX")
    TObjectPtr<UNiagaraSystem> FiringEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> RisingSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> WarningSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> FiringStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> FiringLoopSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> LaserFadeOutSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> FollowRiseSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> LandingSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio")
    TObjectPtr<USoundBase> CircularMissileLaunchSound;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Audio",
        meta = (ClampMin = "0.0", Units = "s"))
    float FiringLoopDelay = 0.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Impact")
    TObjectPtr<UNiagaraSystem> LaserImpactEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Impact", meta = (ClampMin = "0.01"))
    float LaserImpactInterval = 0.1f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "1"))
    float ExitDistance = 2500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0.01"))
    float ExitDuration = 0.6f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0"))
    float JumpHeight = 300.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0"))
    float ExitDropHeight = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0"))
    float OffMapDelay = 1.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "1"))
    float RiseDepth = 5000.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement", meta = (ClampMin = "0.01"))
    float RiseDuration = 3.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Movement")
    TObjectPtr<UCurveFloat> RiseSpeedCurve;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Animation")
    TObjectPtr<UAnimMontage> ChargeMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser", meta = (ClampMin = "1"))
    float TraceLength = 20000.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser", meta = (ClampMin = "1"))
    float BeamWidth = 30.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser", meta = (ClampMin = "0.01"))
    float DamageInterval = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser")
    TEnumAsByte<ECollisionChannel> LaserTraceChannel = ECC_GameTraceChannel6;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UCurveFloat> RotationSpeedCurve;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> WarningMaterial;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> LaserMaterial;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> WarningTriangleMaterial;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Laser")
    TObjectPtr<UMaterialInterface> FiringTriangleMaterial;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Fade")
    TObjectPtr<UCurveFloat> WarningOpacityCurve;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Fade")
    TObjectPtr<UCurveFloat> FiringOpacityCurve;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Fade", meta = (ClampMin = "0.01"))
    float FiringFadeInDuration = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Missiles")
    FBossMissileConfig MissileConfig;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|Missiles")
    FVector MissileSpawnOffset = FVector(0.f, 0.f, 150.f);

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp")
    TObjectPtr<UAnimMontage> FollowRiseMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "1"))
    float FollowRiseHeight = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "0.01"))
    float FollowRiseDuration = 1.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp")
    TObjectPtr<UCurveFloat> FollowRiseSpeedCurve;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp")
    TObjectPtr<class UStaticMesh> DropWarningMesh;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp")
    TObjectPtr<UMaterialInterface> DropWarningMaterial;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "1"))
    float DropDamageDiameter = 400.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "1"))
    float DropWarningHeight = 1600.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "0.01"))
    float DropWarningDuration = 1.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp")
    TObjectPtr<UAnimMontage> DropMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "1"))
    float DropSpeed = 5000.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "0"))
    float DropDamage = 70.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "0"))
    float DropKnockbackHorizontal = 1000.f;

    UPROPERTY(EditAnywhere, Category = "Boss|CrossLaser|FollowUp", meta = (ClampMin = "0"))
    float DropKnockbackUp = 300.f;

    UPROPERTY(EditAnywhere, Category = "Landing")
    FBossLandingAttackBaseSettings Landing;
};

USTRUCT(BlueprintType)
struct FBossJumpAttackSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|Jump")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump")
    TObjectPtr<UNiagaraSystem> ReadyEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump")
    TObjectPtr<UNiagaraSystem> StartEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump|GroundEffect")
    TObjectPtr<UNiagaraSystem> JumpStartGroundEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump|Audio")
    TObjectPtr<USoundBase> JumpStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump|Audio")
    TObjectPtr<USoundBase> LandingSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump|Damage", meta = (ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float JumpGravityMultiplier = 3.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float ApexHeightThreshold = 300.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float PreferredJumpHeight = 350.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float MaxJumpHeight = 600.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float MaxHorizontalSpeed = 2400.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "1.0"))
    float FloorSearchDepth = 3000.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "0.0"))
    float KnockbackHorizontalSpeed = 700.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Jump", meta = (ClampMin = "0.0"))
    float KnockbackUpSpeed = 250.f;

    UPROPERTY(EditAnywhere, Category = "Landing")
    FBossLandingAttackBaseSettings Landing;
};

USTRUCT(BlueprintType)
struct FBossPullAttackSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Animation")
    FName ReadySection = TEXT("PullReady");

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Animation")
    FName LoopSection = TEXT("PullLoop");

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Animation")
    FName EndSection = TEXT("PullEnd");

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Audio")
    TObjectPtr<USoundBase> PatternStartSound;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Audio")
    TObjectPtr<USoundBase> PullLoopEntrySound;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull", meta = (ClampMin = "0.01", Units = "s"))
    float PullDuration = 5.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull", meta = (ClampMin = "1.0", Units = "cm"))
    float PullRange = 1200.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull", meta = (ClampMin = "1.0", Units = "cm"))
    float DamageRange = 350.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull", meta = (ClampMin = "0.0"))
    float PullStrength = 1800.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull", meta = (ClampMin = "1.0", Units = "cm"))
    float RangeHalfHeight = 250.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Damage", meta = (ClampMin = "0.0"))
    float DamagePerTick = 5.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Damage", meta = (ClampMin = "0.01", Units = "s"))
    float DamageInterval = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects")
    TObjectPtr<UNiagaraSystem> RangeEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects")
    TObjectPtr<UNiagaraSystem> DamageAreaEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects")
    FName ScaleParameter = TEXT("User.Scale");

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects")
    FName AlphaParameter = TEXT("User.Alpha");

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects", meta = (ClampMin = "0.01", Units = "s"))
    float RangeFadeInDuration = 1.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects", meta = (ClampMin = "0.01", Units = "cm"))
    float EffectBaseRadius = 100.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Pull|Effects")
    FVector CenterOffset = FVector(0.f, 0.f, -250.f);
};

USTRUCT(BlueprintType)
struct FBossRockThrowSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow")
    TSubclassOf<ABossRockProjectile> RockClass;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow|Audio")
    TObjectPtr<USoundBase> ReadySound;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow|Audio")
    TObjectPtr<USoundBase> ThrowSound;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow|Audio")
    TObjectPtr<USoundBase> RockFlightLoopSound;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow|Audio")
    TObjectPtr<USoundBase> RockImpactSound;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow|Effects")
    TObjectPtr<UNiagaraSystem> ReadyEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow|Effects")
    TObjectPtr<UNiagaraSystem> ThrowEffect;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow", meta = (ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow",
        meta = (ClampMin = "1.0", Units = "cm/s"))
    float RockSpeed = 1800.f;

    UPROPERTY(EditAnywhere, Category = "Boss|RockThrow",
        meta = (ClampMin = "0.1", Units = "s"))
    float RockLifetime = 10.f;
};

UCLASS(BlueprintType)
class THELASTMESSAGE_API UBossCombatData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Stats", meta = (ClampMin = "0.0"))
    float MaxHealth = 0.f;

    UPROPERTY(EditAnywhere, Category = "Stats", meta = (ClampMin = "0.0"))
    float InitialHealth = 0.f;

    UPROPERTY(EditAnywhere, Category = "Stats", meta = (ClampMin = "0.0", Units = "cm/s"))
    float MoveSpeed = 0.f;

    UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = "0.0"))
    float MeleeDamage = 0.f;

    UPROPERTY(EditAnywhere, Category = "Damage")
    TSubclassOf<UGameplayEffect> OutgoingDamageEffect;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossCircularMissileSettings CircularMissile;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossLightningAttackSettings LightningAttack;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossGunBurstSettings GunBurst;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossGunShotSettings GunShot;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossCrossLaserSettings CrossLaser;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossJumpAttackSettings JumpAttack;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossPullAttackSettings PullAttack;

    UPROPERTY(EditAnywhere, Category = "Patterns")
    FBossRockThrowSettings RockThrow;
};