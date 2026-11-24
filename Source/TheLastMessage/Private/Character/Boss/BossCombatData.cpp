#include "Character/Boss/BossCombatData.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/GA_BossCircularMissile.h"
#include "Character/Boss/GA_BossJumpAttack.h"
#include "Character/Boss/GA_BossPullAttack.h"
#include "Character/Boss/GA_BossRockThrow.h"

void UGA_BossAttackBase::OnGiveAbility(
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    const ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    if (IsInstantiated() && IsValid(Boss) && Boss->HasAuthority())
    {
        if (const UBossCombatData* Data = Boss->GetCombatData())
        {
            ApplyCombatData(*Data);
        }
    }

    Super::OnGiveAbility(ActorInfo, Spec);
}

void UGA_BossAttackBase::ApplyCombatData(const UBossCombatData& Data)
{
    DamageEffectClass = Data.OutgoingDamageEffect;
}

void UGA_BossCircularMissile::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossCircularMissileSettings& Settings = Data.CircularMissile;
    AttackMontage = Settings.AttackMontage;
    LaunchNotifyName = Settings.LaunchNotifyName;
    LaunchSound = Settings.LaunchSound;
    MissileSpawnOffset = Settings.MissileSpawnOffset;
}

void UGA_BossLightningAttack::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossLightningAttackSettings& Settings = Data.LightningAttack;
    LightningClass = Settings.LightningClass;
    AttackMontage = Settings.AttackMontage;
    StormEffect = Settings.StormEffect;
    PatternStartSound = Settings.PatternStartSound;
    StormLoopSound = Settings.StormLoopSound;
    StormEndSound = Settings.StormEndSound;
    Phase1Config = Settings.Phase1Config;
    Phase2Config = Settings.Phase2Config;
    Phase3Config = Settings.Phase3Config;
}

void UGA_BossGunBurst::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossGunBurstSettings& Settings = Data.GunBurst;
    Phase1 = Settings.Phase1;
    Phase2 = Settings.Phase2;
    BulletClass = Settings.BulletClass;
    BulletSpeed = Settings.BulletSpeed;
    BulletLifetime = Settings.BulletLifetime;
    MuzzleSocketName = Settings.MuzzleSocketName;
    MuzzleEffect = Settings.MuzzleEffect;
    AttackMontage = Settings.AttackMontage;
    PatternStartSound = Settings.PatternStartSound;
    ShotSound = Settings.ShotSound;
    AimOriginOffset = Settings.AimOriginOffset;
}

void UGA_BossGunShot::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossGunShotSettings& Settings = Data.GunShot;
    Phase1 = Settings.Phase1;
    Phase2 = Settings.Phase2;
    BulletClass = Settings.BulletClass;
    BulletSpeed = Settings.BulletSpeed;
    BulletLifetime = Settings.BulletLifetime;
    MuzzleSocketName = Settings.MuzzleSocketName;
    MuzzleEffect = Settings.MuzzleEffect;
    AttackMontage = Settings.AttackMontage;
    PatternStartSound = Settings.PatternStartSound;
    ShotSound = Settings.ShotSound;
    AimOriginOffset = Settings.AimOriginOffset;
}

void UGA_BossCrossLaser::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossCrossLaserSettings& Settings = Data.CrossLaser;
    Phase1 = Settings.Phase1;
    Phase2 = Settings.Phase2;
    Phase3 = Settings.Phase3;
    JumpMontage = Settings.JumpMontage;
    ReadyFlame = Settings.ReadyFlame;
    FlyFlame = Settings.FlyFlame;
    FlameSocket = Settings.FlameSocket;
    FiringEffect = Settings.FiringEffect;
    PatternStartSound = Settings.PatternStartSound;
    RisingSound = Settings.RisingSound;
    WarningSound = Settings.WarningSound;
    FiringStartSound = Settings.FiringStartSound;
    FiringLoopSound = Settings.FiringLoopSound;
    LaserFadeOutSound = Settings.LaserFadeOutSound;
    FollowRiseSound = Settings.FollowRiseSound;
    LandingSound = Settings.LandingSound;
    CircularMissileLaunchSound = Settings.CircularMissileLaunchSound;
    FiringLoopDelay = Settings.FiringLoopDelay;
    LaserImpactEffect = Settings.LaserImpactEffect;
    LaserImpactInterval = Settings.LaserImpactInterval;
    ExitDistance = Settings.ExitDistance;
    ExitDuration = Settings.ExitDuration;
    JumpHeight = Settings.JumpHeight;
    ExitDropHeight = Settings.ExitDropHeight;
    OffMapDelay = Settings.OffMapDelay;
    RiseDepth = Settings.RiseDepth;
    RiseDuration = Settings.RiseDuration;
    RiseSpeedCurve = Settings.RiseSpeedCurve;
    ChargeMontage = Settings.ChargeMontage;
    TraceLength = Settings.TraceLength;
    BeamWidth = Settings.BeamWidth;
    DamageInterval = Settings.DamageInterval;
    LaserTraceChannel = Settings.LaserTraceChannel;
    RotationSpeedCurve = Settings.RotationSpeedCurve;
    WarningMaterial = Settings.WarningMaterial;
    LaserMaterial = Settings.LaserMaterial;
    WarningTriangleMaterial = Settings.WarningTriangleMaterial;
    FiringTriangleMaterial = Settings.FiringTriangleMaterial;
    WarningOpacityCurve = Settings.WarningOpacityCurve;
    FiringOpacityCurve = Settings.FiringOpacityCurve;
    FiringFadeInDuration = Settings.FiringFadeInDuration;
    MissileConfig = Settings.MissileConfig;
    MissileSpawnOffset = Settings.MissileSpawnOffset;
    FollowRiseMontage = Settings.FollowRiseMontage;
    FollowRiseHeight = Settings.FollowRiseHeight;
    FollowRiseDuration = Settings.FollowRiseDuration;
    FollowRiseSpeedCurve = Settings.FollowRiseSpeedCurve;
    DropWarningMesh = Settings.DropWarningMesh;
    DropWarningMaterial = Settings.DropWarningMaterial;
    DropDamageDiameter = Settings.DropDamageDiameter;
    DropWarningHeight = Settings.DropWarningHeight;
    DropWarningDuration = Settings.DropWarningDuration;
    DropMontage = Settings.DropMontage;
    DropSpeed = Settings.DropSpeed;
    DropDamage = Settings.DropDamage;
    DropKnockbackHorizontal = Settings.DropKnockbackHorizontal;
    DropKnockbackUp = Settings.DropKnockbackUp;
    LandingMontage = Settings.Landing.LandingMontage;
    LandingEffect = Settings.Landing.LandingEffect;
    LandingCameraShakeClass = Settings.Landing.LandingCameraShakeClass;
    LandingCameraShakeScale = Settings.Landing.LandingCameraShakeScale;
    LandingKnockbackRadius = Settings.Landing.LandingKnockbackRadius;
    LandingKnockbackSpeed = Settings.Landing.LandingKnockbackSpeed;
    FacingTurnSpeedCurve = Settings.Landing.FacingTurnSpeedCurve;
    FacingTurnDuration = Settings.Landing.FacingTurnDuration;
}

void UGA_BossJumpAttack::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossJumpAttackSettings& Settings = Data.JumpAttack;
    AttackMontage = Settings.AttackMontage;
    ReadyEffect = Settings.ReadyEffect;
    StartEffect = Settings.StartEffect;
    JumpStartGroundEffect = Settings.JumpStartGroundEffect;
    PatternStartSound = Settings.PatternStartSound;
    JumpStartSound = Settings.JumpStartSound;
    LandingSound = Settings.LandingSound;
    Damage = Settings.Damage;
    JumpGravityMultiplier = Settings.JumpGravityMultiplier;
    ApexHeightThreshold = Settings.ApexHeightThreshold;
    PreferredJumpHeight = Settings.PreferredJumpHeight;
    MaxJumpHeight = Settings.MaxJumpHeight;
    MaxHorizontalSpeed = Settings.MaxHorizontalSpeed;
    FloorSearchDepth = Settings.FloorSearchDepth;
    KnockbackHorizontalSpeed = Settings.KnockbackHorizontalSpeed;
    KnockbackUpSpeed = Settings.KnockbackUpSpeed;
    LandingMontage = Settings.Landing.LandingMontage;
    LandingEffect = Settings.Landing.LandingEffect;
    LandingCameraShakeClass = Settings.Landing.LandingCameraShakeClass;
    LandingCameraShakeScale = Settings.Landing.LandingCameraShakeScale;
    LandingKnockbackRadius = Settings.Landing.LandingKnockbackRadius;
    LandingKnockbackSpeed = Settings.Landing.LandingKnockbackSpeed;
    FacingTurnSpeedCurve = Settings.Landing.FacingTurnSpeedCurve;
    FacingTurnDuration = Settings.Landing.FacingTurnDuration;
}

void UGA_BossPullAttack::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossPullAttackSettings& Settings = Data.PullAttack;
    AttackMontage = Settings.AttackMontage;
    ReadySection = Settings.ReadySection;
    LoopSection = Settings.LoopSection;
    EndSection = Settings.EndSection;
    PatternStartSound = Settings.PatternStartSound;
    PullLoopEntrySound = Settings.PullLoopEntrySound;
    PullDuration = Settings.PullDuration;
    PullRange = Settings.PullRange;
    DamageRange = Settings.DamageRange;
    PullStrength = Settings.PullStrength;
    RangeHalfHeight = Settings.RangeHalfHeight;
    DamagePerTick = Settings.DamagePerTick;
    DamageInterval = Settings.DamageInterval;
    RangeEffect = Settings.RangeEffect;
    DamageAreaEffect = Settings.DamageAreaEffect;
    ScaleParameter = Settings.ScaleParameter;
    AlphaParameter = Settings.AlphaParameter;
    RangeFadeInDuration = Settings.RangeFadeInDuration;
    EffectBaseRadius = Settings.EffectBaseRadius;
    CenterOffset = Settings.CenterOffset;
}

void UGA_BossRockThrow::ApplyCombatData(const UBossCombatData& Data)
{
    Super::ApplyCombatData(Data);
    const FBossRockThrowSettings& Settings = Data.RockThrow;
    AttackMontage = Settings.AttackMontage;
    RockClass = Settings.RockClass;
    ReadySound = Settings.ReadySound;
    ThrowSound = Settings.ThrowSound;
    RockFlightLoopSound = Settings.RockFlightLoopSound;
    RockImpactSound = Settings.RockImpactSound;
    ReadyEffect = Settings.ReadyEffect;
    ThrowEffect = Settings.ThrowEffect;
    Damage = Settings.Damage;
    RockSpeed = Settings.RockSpeed;
    RockLifetime = Settings.RockLifetime;
}