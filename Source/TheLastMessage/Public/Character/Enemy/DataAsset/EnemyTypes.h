#pragma once

#include "CoreMinimal.h"
#include "EnemyTypes.generated.h"

class AEnemyProjectile;

UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	Cinder,
	Sentry,
	Flea,
	Rattler,
	Juggernaut,
	Junker
};

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Hidden,    // Junker 전용: 등장 전 대기
	Patrol,    // 대기 정찰
	Chase,     // 플레이어에게 이동
	Attack,    // 공격 중
	Dead       // 사망
};

UENUM(BlueprintType)
enum class EEnemyAttackType : uint8
{
	None,
	Ranged,        // 원거리 (Cinder, Sentry, Rattler, Juggernaut, Junker)
	Melee,         // 근접 (Junker)
	AreaMelee,     // 근접 범위 전파 (Flea)
	Charge,        // 돌진 (Juggernaut)
	SelfDestruct   // 자폭 (Cinder 낙하 후)
};

USTRUCT(BlueprintType)
struct FEnemyStatConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeed = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DetectRange = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackRange = 200.f;

	/** 추격 중 타겟에게 이만큼 가까워지면 멈추는 거리(Move To 허용 반경). 공격 범위보다 작게 둔다. 0 이하이면 공격 범위의 80%를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChaseAcceptableRadius = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackDamage = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackCooldown = 1.f;
};

USTRUCT(BlueprintType)
struct FEnemyProjectileConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AEnemyProjectile> ProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Lifetime = 5.f;
};
