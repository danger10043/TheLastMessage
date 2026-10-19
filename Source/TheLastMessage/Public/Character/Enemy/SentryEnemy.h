#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "SentryEnemy.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/** 에너지포 봇: 이동 없이 고정된 채 조준 → 충전 → 발사를 반복하는 포탑. 느리고 강력한 원거리 공격. */
UCLASS()
class THELASTMESSAGE_API ASentryEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ASentryEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual void StartAttack() override;
	virtual void CancelAttack() override;
	virtual void ResetEnemy() override;

	/**
	 * 타겟을 향한 조준 각도(AimYaw, AimPitch)를 갱신한다.
	 * 몸통은 회전하지 않고, 애니메이션 블루프린트가 이 값을 총 본(GunHor, GunVert)에 적용한다.
	 */
	void RotateToTarget(float DeltaTime);

	/** 조준 방향 좌우(도). 몸체 정면 기준, 애니메이션 블루프린트가 GunHor 본에 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Sentry|Aim")
	float AimYaw = 0.f;

	/** 조준 방향 상하(도). 위쪽이 양수, 애니메이션 블루프린트가 GunVert 본에 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Sentry|Aim")
	float AimPitch = 0.f;

	/** 충전을 시작한다. */
	void StartCharge();

	/** 충전이 끝나면 투사체를 발사한다. */
	void FireProjectile();

	/** 충전을 중단한다(타겟 이탈·사망 시). */
	void CancelCharge();

	/** 블루프린트에서 충전·발사 연출(이펙트·사운드)을 붙이기 위한 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sentry")
	void OnChargeStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sentry")
	void OnChargeCancelled();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sentry")
	void OnFired();

protected:
	virtual void BeginPlay() override;

	/** 느리고 강력한 투사체 설정 (피해량 높게, 속도 느리게) */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry")
	FEnemyProjectileConfig ProjectileConfig;

	/** 발사 전 충전 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry")
	float ChargeTime = 1.2f;

	/** 조준 회전 속도(도/초) */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Aim")
	float RotationSpeed = 120.f;

	/** 조준 상하 각도 범위(도) */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Aim")
	float MinAimPitch = -20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Aim")
	float MaxAimPitch = 70.f;

	/** 총이 정면이 아닌 쪽을 보면 이 값으로 좌우 기준을 보정한다(도). */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Aim")
	float AimYawOffset = 0.f;

	/** 조준 중심으로 쓸 본 이름(총 상하 회전축) */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Aim")
	FName AimPivotBoneName = TEXT("GunVert");

	/** 투사체·발사 화염이 나갈 스켈레탈 메시 소켓 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry")
	FName MuzzleSocketName = TEXT("SFire");

	/** 충전 중 총구에 붙어 재생될 이펙트. 반복(Looping) 시스템으로 만든다. 발사·취소 시 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Effects")
	TObjectPtr<UNiagaraSystem> ChargeEffect;

	/** 발사 순간 총구에서 재생될 화염 이펙트 */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry|Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** 총구 소켓이 없을 때 쓰는 발사 위치(포탑 기준 로컬 좌표) */
	UPROPERTY(EditDefaultsOnly, Category = "Sentry")
	FVector MuzzleOffset = FVector(120.f, 0.f, 0.f);

	UPROPERTY(VisibleInstanceOnly, Category = "Sentry")
	bool bIsCharging = false;

private:
	void StopChargeEffect();

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ChargeEffectComponent;

	FTimerHandle ChargeTimerHandle;
};
