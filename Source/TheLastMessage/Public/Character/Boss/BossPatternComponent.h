#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Boss/BossTypes.h"
#include "BossPatternComponent.generated.h"

UCLASS()
class THELASTMESSAGE_API UBossPatternComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void ResetPattern();

    EBossAttackType SelectAttackPattern(EBossPhase Phase, float TargetDistance);

    bool ExecuteAttackPattern(EBossAttackType AttackType);

    bool CanExecuteAttack(EBossAttackType AttackType);

    TArray<EBossAttackType> GetAvailableAttacks(EBossPhase Phase);

    EBossAttackType SelectNextMissileAttack();

    FBossMissileConfig GetMissileConfig(EBossPhase Phase);

    bool IsDistanceInAttackRange(EBossAttackType AttackType, float TargetDistance) const;

    // Returns -1 when configuration is invalid; 0 means ready.
    float GetAttackCooldownRemaining(EBossAttackType AttackType) const;

    bool TryStartAttackCooldown(EBossAttackType AttackType);

    void CancelCurrentAttack();

private:
    // Runtime state per boss and attack type. Not cleared on target loss/cancel.
    TMap<EBossAttackType, double> CooldownEndTimes;

    UPROPERTY()
    EBossAttackType CurrentAttackType = EBossAttackType::None;

    UPROPERTY(EditDefaultsOnly)
    TArray<FBossAttackConfig> AttackConfigs;

    UPROPERTY(EditDefaultsOnly)
    FBossMissileConfig Phase1MissileConfig;

    UPROPERTY(EditDefaultsOnly)
    FBossMissileConfig Phase2MissileConfig;

    UPROPERTY(EditDefaultsOnly)
    FBossMissileConfig Phase3MissileConfig;

    // 첫 미사일 종류는 명세 미기재. 원형부터 시작하는 제안이다.
    UPROPERTY()
    bool bUseCircularMissileNext = true;

    UPROPERTY()
    bool bIsAttackExecuting = false;
};