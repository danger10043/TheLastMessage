#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "RattlerEnemy.generated.h"

/** 덩치봇 1: 땅을 기어다니며 원거리 연발 공격 */
UCLASS()
class THELASTMESSAGE_API ARattlerEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ARattlerEnemy();

	virtual void StartAttack() override;
	virtual void CancelAttack() override;

	void StartBurst();
	void FireOnce();
	void EndBurst();

protected:
	virtual float GetAttackCooldown() const override { return BurstCooldown; }

	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	FEnemyProjectileConfig ProjectileConfig;

	/** 한 번의 연발에서 발사하는 개수 */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	int32 BurstCount = 3;

	/** 연발 중 발사 간격(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	float FireInterval = 0.15f;

	/** 연발 종료 후 다음 공격까지 대기 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	float BurstCooldown = 1.5f;

	/** 투사체 발사 위치(몬스터 기준 로컬 좌표) */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	UPROPERTY(VisibleInstanceOnly, Category = "Rattler")
	int32 RemainingBurst = 0;

private:
	FTimerHandle BurstTimerHandle;
};
