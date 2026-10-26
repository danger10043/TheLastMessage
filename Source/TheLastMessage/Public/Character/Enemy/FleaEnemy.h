#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "FleaEnemy.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

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
	virtual void ApplyEnemyData(const UEnemyDataAsset* Data) override;

	/** 범위 공격 반경 */
	float AreaAttackRadius = 250.f;

	/**
	 * 충전 이펙트. 공격 시작부터 판정이 터지기 전(Area Attack Windup 동안)까지 재생된다.
	 * 반복(Looping) 시스템으로 만들면 판정 순간·공격 취소 시 자동으로 멈춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Flea|Effects")
	TObjectPtr<UNiagaraSystem> ChargeEffect;

	/** 충전 이펙트를 붙일 스켈레탈 메시 소켓. 비우거나 없으면 몬스터 기준 오프셋에 붙는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Flea|Effects")
	FName ChargeEffectSocketName;

	/** 소켓이 없을 때 충전 이펙트를 붙일 위치(몬스터 기준 로컬 좌표) */
	FVector ChargeEffectOffset = FVector::ZeroVector;

	/** 켜면 충전 이펙트 크기를 범위 공격 반경에 맞춰 키운다(위험 범위 표시용). 기준 반경은 Shockwave Effect Base Radius를 쓴다. */
	bool bScaleChargeEffectToAttackRadius = false;

	/** 범위 공격 충격파 이펙트 (발밑에서 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "Flea|Effects")
	TObjectPtr<UNiagaraSystem> ShockwaveEffect;

	/** 충격파 이펙트가 원래 만들어진 반경. 실제 반경(AreaAttackRadius)에 맞춰 크기를 조절하는 기준이다. */
	float ShockwaveEffectBaseRadius = 250.f;

	/** 공격 시작부터 판정까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	float AreaAttackWindup = 0.4f;

private:
	void StopChargeEffect();

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ChargeEffectComponent;

	FTimerHandle AreaAttackTimerHandle;
};
