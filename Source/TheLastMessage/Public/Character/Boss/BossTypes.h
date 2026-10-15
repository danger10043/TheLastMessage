#pragma once

#include "CoreMinimal.h"
#include "BossTypes.generated.h"

class UGameplayAbility;
class ABossMissile;

UENUM(BlueprintType)
enum class EBossPhase : uint8
{
    Phase1,
    Phase2,
    Phase3
};

UENUM(BlueprintType)
enum class EBossState : uint8
{
    Idle,
    Combat,
    Attacking,
    ShieldPuzzle,
    Cinematic,
    Dead
};

UENUM(BlueprintType)
enum class EBossPuzzleType : uint8
{
    None,
    LaserPuzzle,
    CorePuzzle
};

UENUM(BlueprintType)
enum class EBossAttackType : uint8
{
    None,
    Melee,
    CircularMissile,
    FallingMissile,
    ShieldCharge,
    GunBurst,
    PlatformMissile,
    JumpAttack,
    RockThrow,
    PullAttack
};

USTRUCT()
struct THELASTMESSAGE_API FBossAttackConfig
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    EBossAttackType AttackType = EBossAttackType::None;

    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UGameplayAbility> AbilityClass;

    // 임시 초기값. 실제 값은 공격 설정에서 지정한다.
    UPROPERTY(EditDefaultsOnly)
    float Cooldown = 0.f;

    UPROPERTY(EditDefaultsOnly)
    float MinRange = 0.f;

    UPROPERTY(EditDefaultsOnly)
    float MaxRange = 0.f;

    UPROPERTY(EditDefaultsOnly)
    float SelectionWeight = 0.f;
};

USTRUCT()
struct THELASTMESSAGE_API FBossMissileConfig
{
    GENERATED_BODY()

    // 명세에 수치 미기재. 설정 전에는 미사일을 생성하지 않는다.
    UPROPERTY(EditDefaultsOnly)
    int32 MissileCount = 0;

    UPROPERTY(EditDefaultsOnly)
    float Damage = 0.f;

    UPROPERTY(EditDefaultsOnly)
    float SpawnInterval = 0.f;

    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<ABossMissile> MissileClass;
};