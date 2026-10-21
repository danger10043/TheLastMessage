#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "JuggernautEnemy.generated.h"

class UNiagaraSystem;

/**
 * 덩치봇 2: 플레이어 원거리 공격에 받는 피해가 줄어드는(기본 50%) 돌진형 몬스터.
 *
 * 공격 흐름: 윈드업(제자리에서 예고) → 돌진(방향 고정, 직선 이동) → 후딜(경직) → 쿨다운
 * - 돌진 방향은 돌진이 시작되는 순간 정해진다. 윈드업 동안 플레이어가 위치를 바꾸면 피할 수 있다.
 * - 돌진은 길을 찾는 이동(NavMesh)이 아니라 직접 이동이라 낭떠러지에서 멈추지 않고 그대로 떨어진다.
 *   떨어진 뒤 착지한 곳에 NavMesh가 있어야 이후 AI가 다시 동작한다.
 */
UCLASS()
class THELASTMESSAGE_API AJuggernautEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AJuggernautEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual void StartAttack() override;
	virtual void CancelAttack() override;

	/** 지금 돌진 중인지(애님 블루프린트에서 돌진 애니메이션 전환에 사용) */
	UFUNCTION(BlueprintPure, Category = "Juggernaut")
	bool IsCharging() const { return bCharging; }

protected:
	virtual void BeginPlay() override;
	virtual float GetAttackCooldown() const override { return ChargeCooldown; }

	/** 돌진 전 예고 시간(초). 이 동안 제자리에서 플레이어를 보고 서 있다. 플레이어가 피할 시간이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeWindup = 0.8f;

	/** 돌진 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeSpeed = 1400.f;

	/** 돌진 중 가속도. 크게 두어 돌진 속도에 즉시 도달하게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeAcceleration = 20000.f;

	/** 바닥에서 돌진하는 최대 거리(좌우 방향). 공중에 있는 동안은 거리에 포함되지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeMaxDistance = 1500.f;

	/** 돌진 최대 시간(초). 어떤 경우에도 이 시간이 지나면 돌진이 끝난다(안전장치). */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.1"))
	float ChargeMaxDuration = 4.f;

	/** 돌진으로 플레이어에게 주는 피해량(레벨 배율 적용 전) */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge")
	float ChargeDamage = 30.f;

	/** 피해 판정 범위를 캡슐보다 얼마나 넓힐지 */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeHitMargin = 20.f;

	/** 켜면 플레이어를 맞히는 순간 돌진이 끝난다. 끄면 맞힌 뒤에도 계속 달린다(한 번의 돌진에서 같은 대상은 한 번만 맞는다). */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge")
	bool bEndChargeOnHit = true;

	/**
	 * 켜면 돌진하는 동안 다른 폰(몬스터, 플레이어)에 몸이 막히지 않고 통과한다.
	 * 피해 판정은 별도로 겹침 검사로 하므로 플레이어는 그대로 맞는다. 끝나면 원래 충돌 설정으로 돌아간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge")
	bool bPassThroughPawnsWhileCharging = true;

	/** 돌진이 끝난 뒤 움직이지 못하는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeRecoveryTime = 1.f;

	/** 벽에 부딪혀 끝났을 때의 경직 시간(초). 벽 박치기는 더 오래 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float WallStunTime = 1.8f;

	/** 바닥에서 속도가 돌진 속도의 이 비율 아래로 떨어지면 벽에 막힌 것으로 본다. */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallHitSpeedRatio = 0.25f;

	/** 다음 돌진까지의 쿨다운(초). 후딜이 끝난 시점부터 계산한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float ChargeCooldown = 3.f;

	/** 플레이어를 맞혔을 때 밀어내는 수평 속도(0이면 밀어내지 않음) */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float KnockbackHorizontal = 0.f;

	/** 플레이어를 맞혔을 때 위로 띄우는 속도(0이면 띄우지 않음) */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Charge", meta = (ClampMin = "0.0"))
	float KnockbackUp = 0.f;

	/** 윈드업 시작 때 재생할 이펙트(예: 땅을 긁는 먼지, 경고 표시) */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Effects")
	TObjectPtr<UNiagaraSystem> WindupEffect;

	/** 플레이어나 벽에 부딪힐 때 재생할 이펙트 */
	UPROPERTY(EditDefaultsOnly, Category = "Juggernaut|Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

private:
	void BeginCharge();
	void EndCharge(bool bHitWall);

	/** 돌진 상태를 해제하고 이동 설정과 틱을 원래대로 되돌린다. */
	void StopCharging();

	/** 돌진 중 플레이어와 겹쳤는지 확인하고 피해를 준다. 맞혔으면 true. */
	bool TryHitPlayers();

	/** 후딜이 끝났을 때: AI를 다시 실행하고 공격을 마친다. */
	void FinishRecovery();

	void PauseAI();
	void ResumeAI();

	bool bCharging = false;

	/** 돌진 중 폰 충돌을 겹침으로 바꿨는지, 그리고 원래 설정 */
	bool bPawnResponseOverridden = false;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
	FVector ChargeDirection = FVector::ForwardVector;
	float ChargeStartX = 0.f;
	float ChargeElapsed = 0.f;
	float DefaultMaxAcceleration = 2048.f;

	/** 한 번의 돌진에서 이미 맞은 대상 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	FTimerHandle WindupTimerHandle;
	FTimerHandle RecoveryTimerHandle;
};
