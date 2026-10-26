#pragma once

#include "CoreMinimal.h"
#include "EnemySettings.generated.h"

/*
 * 몬스터 종류별 수치 묶음. UEnemyDataAsset 안에 종류별로 하나씩 들어 있다.
 * 몬스터는 자기 종류의 묶음만 읽는다.
 */

/** Rattler의 수치. 몽타주·이펙트·소켓 이름 같은 에셋 참조는 몬스터 블루프린트에 둔다. */
USTRUCT(BlueprintType)
struct FRattlerSettings
{
	GENERATED_BODY()

	/** 한 번의 연발에서 발사하는 개수 */
	UPROPERTY(EditAnywhere)
	int32 BurstCount = 3;

	/** 연발 중 발사 간격(초) */
	UPROPERTY(EditAnywhere)
	float FireInterval = 0.15f;

	/** 연발 종료 후 다음 공격까지 대기 시간(초) */
	UPROPERTY(EditAnywhere)
	float BurstCooldown = 1.5f;

	/** 투사체 발사 위치(몬스터 기준 로컬 좌표) */
	UPROPERTY(EditAnywhere)
	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	/** 투사체 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 1500.f;

	/** 투사체 수명(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 5.f;
};

/** Flea의 수치. 몽타주·이펙트·소켓 이름 같은 에셋 참조는 몬스터 블루프린트에 둔다. */
USTRUCT(BlueprintType)
struct FFleaSettings
{
	GENERATED_BODY()

	/** 범위 공격 반경 */
	UPROPERTY(EditAnywhere)
	float AreaAttackRadius = 250.f;

	/** 소켓이 없을 때 충전 이펙트를 붙일 위치(몬스터 기준 로컬 좌표) */
	UPROPERTY(EditAnywhere)
	FVector ChargeEffectOffset = FVector::ZeroVector;

	/** 켜면 충전 이펙트 크기를 범위 공격 반경에 맞춰 키운다(위험 범위 표시용). 기준 반경은 Shockwave Effect Base Radius를 쓴다. */
	UPROPERTY(EditAnywhere)
	bool bScaleChargeEffectToAttackRadius = false;

	/** 충격파 이펙트가 원래 만들어진 반경. 실제 반경(AreaAttackRadius)에 맞춰 크기를 조절하는 기준이다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float ShockwaveEffectBaseRadius = 250.f;

	/** 공격 시작부터 판정까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	UPROPERTY(EditAnywhere)
	float AreaAttackWindup = 0.4f;
};

/** Sentry의 수치. 몽타주·이펙트·소켓 이름 같은 에셋 참조는 몬스터 블루프린트에 둔다. */
USTRUCT(BlueprintType)
struct FSentrySettings
{
	GENERATED_BODY()

	/** 발사 전 충전 시간(초) */
	UPROPERTY(EditAnywhere)
	float ChargeTime = 1.2f;

	/** 조준 회전 속도(도/초) */
	UPROPERTY(EditAnywhere)
	float RotationSpeed = 120.f;

	/** 조준 상하 각도 범위(도) */
	UPROPERTY(EditAnywhere)
	float MinAimPitch = -20.f;

	UPROPERTY(EditAnywhere)
	float MaxAimPitch = 70.f;

	/** 총이 정면이 아닌 쪽을 보면 이 값으로 좌우 기준을 보정한다(도). */
	UPROPERTY(EditAnywhere)
	float AimYawOffset = 0.f;

	/** 총구 소켓이 없을 때 쓰는 발사 위치(포탑 기준 로컬 좌표) */
	UPROPERTY(EditAnywhere)
	FVector MuzzleOffset = FVector(120.f, 0.f, 0.f);

	/** 투사체 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 700.f;

	/** 투사체 수명(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 6.f;
};

/** Cinder의 수치. 몽타주·이펙트·소켓 이름 같은 에셋 참조는 몬스터 블루프린트에 둔다. */
USTRUCT(BlueprintType)
struct FCinderSettings
{
	GENERATED_BODY()

	/** 몸 중심이 표면에서 떨어져 있는 거리. 캡슐/메시 크기에 맞춰 조정한다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float SurfaceOffset = 60.f;

	/** 표면을 찾을 때 SurfaceOffset 외에 더 확인하는 거리 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float SurfaceSnapDistance = 40.f;

	/** 앞쪽 장애물(오목한 모서리)을 미리 확인하는 거리 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float CornerCheckDistance = 40.f;

	/** 스폰될 때 가장 가까운 표면을 찾는 최대 거리 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float SpawnSnapDistance = 300.f;

	/** 켜면 움직이는 물체(월드 다이나믹)도 기어갈 표면으로 인식한다. 끄면 고정된 지형(월드 스태틱)만 인식한다. */
	UPROPERTY(EditAnywhere)
	bool bUseWorldDynamicSurfaces = false;

	/** 몸이 표면 방향으로 돌아가는 속도(클수록 빠름, 0이면 즉시) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 12.f;

	/** 모서리를 돌 때 몸이 위치를 따라가는 속도(클수록 빠름, 0이면 즉시). 낮추면 모서리에서 부드럽게 넘어간다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PositionInterpSpeed = 15.f;

	/** 한 번 이동하는 거리의 최솟값·최댓값 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PatrolMinDistance = 300.f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PatrolMaxDistance = 800.f;

	/** 이동 사이에 멈춰 있는 시간의 최솟값·최댓값(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PatrolWaitMin = 1.f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PatrolWaitMax = 2.5f;

	/** 공격 시작부터 발사까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float FireDelay = 0.3f;

	UPROPERTY(EditAnywhere)
	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	/** 체력 비율이 이 값 이하가 되면 바닥으로 떨어진다 (0.5 = 50%) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FallHealthRatio = 0.5f;

	/** 착지 후 플레이어에게 달려가는 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float FallenMoveSpeed = 700.f;

	/** 플레이어와 몸 가장자리 사이 거리가 이 값 이하가 되면 자폭 준비를 시작한다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ExplosionTriggerRange = 150.f;

	/** 자폭 준비를 시작하고 폭발할 때까지의 시간(초). 이 동안 멈춰서 예고한다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ExplosionFuseTime = 0.4f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ExplosionRadius = 250.f;

	/** 폭발 이펙트 크기 배율. 이펙트가 너무 크거나 작으면 이 값으로 조절한다(1 = 원래 크기). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float ExplosionEffectScale = 1.f;

	/** 자폭 준비 이펙트 크기 배율 (1 = 원래 크기) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float FuseEffectScale = 1.f;

	/** 정찰 이동 축(월드 기준). 사이드뷰 기본은 X축 왕복. 벽·천장 이동형은 (0,0,1)로 설정. */
	UPROPERTY(EditAnywhere)
	FVector PatrolAxis = FVector(0.f, 0.f, 1.f);

	/** 투사체 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 900.f;

	/** 투사체 수명(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 5.f;
};

/** Juggernaut의 수치. 몽타주·이펙트·소켓 이름 같은 에셋 참조는 몬스터 블루프린트에 둔다. */
USTRUCT(BlueprintType)
struct FJuggernautSettings
{
	GENERATED_BODY()

	/** 돌진 전 예고 시간(초). 이 동안 제자리에서 플레이어를 보고 서 있다. 플레이어가 피할 시간이다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeWindup = 0.8f;

	/** 돌진 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeSpeed = 1400.f;

	/** 돌진 중 가속도. 크게 두어 돌진 속도에 즉시 도달하게 한다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeAcceleration = 20000.f;

	/** 바닥에서 돌진하는 최대 거리(좌우 방향). 공중에 있는 동안은 거리에 포함되지 않는다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeMaxDistance = 1500.f;

	/** 돌진 최대 시간(초). 어떤 경우에도 이 시간이 지나면 돌진이 끝난다(안전장치). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float ChargeMaxDuration = 4.f;

	/** 피해 판정 범위를 캡슐보다 얼마나 넓힐지 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeHitMargin = 20.f;

	/** 켜면 플레이어를 맞히는 순간 돌진이 끝난다. 끄면 맞힌 뒤에도 계속 달린다(한 번의 돌진에서 같은 대상은 한 번만 맞는다). */
	UPROPERTY(EditAnywhere)
	bool bEndChargeOnHit = true;

	/**
	 * 켜면 돌진하는 동안 다른 폰(몬스터, 플레이어)에 몸이 막히지 않고 통과한다.
	 * 피해 판정은 별도로 겹침 검사로 하므로 플레이어는 그대로 맞는다. 끝나면 원래 충돌 설정으로 돌아간다.
	 */
	UPROPERTY(EditAnywhere)
	bool bPassThroughPawnsWhileCharging = true;

	/** 돌진이 끝난 뒤 움직이지 못하는 시간(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeRecoveryTime = 1.f;

	/** 벽에 부딪혀 끝났을 때의 경직 시간(초). 벽 박치기는 더 오래 멈춘다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float WallStunTime = 1.8f;

	/** 바닥에서 속도가 돌진 속도의 이 비율 아래로 떨어지면 벽에 막힌 것으로 본다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallHitSpeedRatio = 0.25f;

	/** 다음 돌진까지의 쿨다운(초). 후딜이 끝난 시점부터 계산한다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ChargeCooldown = 3.f;

	/** 플레이어를 맞혔을 때 밀어내는 수평 속도(0이면 밀어내지 않음) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float KnockbackHorizontal = 0.f;

	/** 플레이어를 맞혔을 때 위로 띄우는 속도(0이면 띄우지 않음) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float KnockbackUp = 0.f;

	/**
	 * 플레이어 원거리 피해(Damage.Category.Player.Ranged)에 곱할 배율. 1이면 그대로, 0.5면 절반으로 줄어든다.
	 * 근접 피해에는 적용되지 않는다.
	 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RangedDamageMultiplier = 0.5f;
};

/** Junker의 수치. 몽타주·이펙트·소켓 이름 같은 에셋 참조는 몬스터 블루프린트에 둔다. */
USTRUCT(BlueprintType)
struct FJunkerSettings
{
	GENERATED_BODY()

	/** 한 번의 점사에서 쏘는 발수 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 ShotsPerBurst = 4;

	/** 점사 안에서 발사 간격(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.01"))
	float FireInterval = 0.12f;

	/** 점사와 점사 사이의 간격(초). 마지막 점사 뒤에도 이만큼 기다린 뒤 다음 단계로 넘어간다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float BurstGap = 0.6f;

	/** 마지막 발사 후 사격 몽타주를 끊고 다음 단계(돌격)로 넘어가기까지의 대기 시간(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float EndDelay = 0.1f;

	/** 사격 몽타주를 끊을 때의 블렌드 아웃 시간(초). 짧을수록 바로 달리기 자세로 넘어간다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ShootMontageBlendOut = 0.15f;

	/** 한 단계에서 쏘는 점사 횟수의 최솟값·최댓값(매번 무작위) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 MinBursts = 2;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 MaxBursts = 3;

	/** 원거리 단계에서 다음 공격 시작까지의 쿨다운(초). 돌격 제한 시간이 끝나 원거리로 돌아왔을 때 적용된다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RangedCooldown = 1.f;

	UPROPERTY(EditAnywhere)
	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	/** 돌격이 유지되는 최대 시간(초). 이 안에 근접 범위에 닿지 못하면 원거리 단계로 돌아간다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float RushDuration = 4.f;

	/** 돌격 중 이 시간(초) 동안 거의 움직이지 못하고 근접 범위에도 못 들어오면 돌격을 포기하고 원거리 단계로 돌아간다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float RushStallTimeLimit = 0.6f;

	/** 돌격 중 이동 속도 배율(기본 이동 속도 기준) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float RushSpeedMultiplier = 1.5f;

	/**
	 * 돌격 중 플레이어와 몸 가장자리(캡슐 표면) 사이 거리가 이 값 안이면 근접 연타를 시작한다.
	 * 추격이 멈추는 거리(Chase Acceptable Radius)보다 커야 한다. 작으면 추격이 멈춘 뒤에도 근접이 시작되지 않는다.
	 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MeleeRange = 200.f;

	/** 한 번의 근접 단계에서 주먹질하는 횟수 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 MeleeComboCount = 3;

	/** 주먹질 시작부터 다음 주먹질 시작까지의 간격(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.05"))
	float MeleeInterval = 0.7f;

	/** 주먹질 시작부터 판정까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MeleeHitDelay = 0.3f;

	/** 판정 구체의 중심까지 거리(몬스터 앞쪽)와 반경 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MeleeReach = 120.f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float MeleeRadius = 120.f;

	/** 후퇴 사격 중 뒤로 물러나는 최대 거리. 이 거리만큼 물러나면 그 자리에서 사격만 계속한다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RetreatMaxDistance = 600.f;

	/** 후퇴 중 몸 앞쪽(물러나는 방향) 바닥을 확인하는 거리. 캡슐 가장자리에서 이만큼 앞에 바닥이 없으면 더 물러나지 않는다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RetreatLedgeCheckDistance = 80.f;

	/** 후퇴 중 앞쪽 바닥이 발밑보다 이 값 이상 낮으면 낭떠러지로 보고 물러나지 않는다. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RetreatMaxDropHeight = 100.f;

	/** 후퇴 사격 중 뒤로 이동하는 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RetreatSpeed = 300.f;

	/** 투사체 속도 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 1500.f;

	/** 투사체 수명(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 5.f;
};

