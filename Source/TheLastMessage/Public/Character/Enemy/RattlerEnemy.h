#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "RattlerEnemy.generated.h"

class UNiagaraSystem;
class USoundBase;

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
	virtual void ApplyEnemyData(const UEnemyDataAsset* Data) override;

	virtual float GetAttackCooldown() const override { return BurstCooldown; }

	/** 발사할 투사체 블루프린트. 속도·수명은 데이터 에셋에서 정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	/** 투사체 설정(클래스는 위 ProjectileClass, 속도·수명은 데이터 에셋에서 복사된다) */
	FEnemyProjectileConfig ProjectileConfig;

	/** 한 번의 연발에서 발사하는 개수 */
	int32 BurstCount = 3;

	/** 연발 중 발사 간격(초) */
	float FireInterval = 0.15f;

	/** 연발 종료 후 다음 공격까지 대기 시간(초) */
	float BurstCooldown = 1.5f;

	/** 발사할 때 재생할 소리. 총구 위치에서 재생된다. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler|Sound")
	TObjectPtr<USoundBase> FireSound;

	/** 총구 화염 이펙트 (한 발마다 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler|Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** 투사체·총구 화염이 나갈 스켈레탈 메시 소켓 이름. 소켓이 없으면 아래 MuzzleOffset을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Rattler")
	FName MuzzleSocketName = TEXT("Fire");

	/** 투사체 발사 위치(몬스터 기준 로컬 좌표) */
	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	UPROPERTY(VisibleInstanceOnly, Category = "Rattler")
	int32 RemainingBurst = 0;

private:
	FTimerHandle BurstTimerHandle;

	/** 이번 연발의 첫 발을 쏠 때 정한 조준 방향. 연발 내내 이 방향으로 쏜다. */
	FVector LockedAimDirection = FVector::ZeroVector;
	bool bAimLocked = false;
};
