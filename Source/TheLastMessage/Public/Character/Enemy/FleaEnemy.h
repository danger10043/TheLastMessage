#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "FleaEnemy.generated.h"

class UNiagaraSystem;

/** 눈깔봇: 땅을 기어다니며 근접 범위 전파 공격. 체력이 높다. */
UCLASS()
class THELASTMESSAGE_API AFleaEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AFleaEnemy();

	virtual void StartAttack() override;
	virtual void CancelAttack() override;

	void StartAreaAttack();
	void ExecuteAreaAttack();
	void SpawnShockwaveEffect();

protected:
	/** 범위 공격 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "Flea")
	float AreaAttackRadius = 250.f;

	/** 범위 공격 피해량 */
	UPROPERTY(EditDefaultsOnly, Category = "Flea")
	float AreaAttackDamage = 15.f;

	/** 범위 공격 충격파 이펙트 (발밑에서 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "Flea|Effects")
	TObjectPtr<UNiagaraSystem> ShockwaveEffect;

	/** 충격파 이펙트가 원래 만들어진 반경. 실제 반경(AreaAttackRadius)에 맞춰 크기를 조절하는 기준이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Flea|Effects", meta = (ClampMin = "1.0"))
	float ShockwaveEffectBaseRadius = 250.f;

	/** 공격 시작부터 판정까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	UPROPERTY(EditDefaultsOnly, Category = "Flea")
	float AreaAttackWindup = 0.4f;

private:
	FTimerHandle AreaAttackTimerHandle;
};
