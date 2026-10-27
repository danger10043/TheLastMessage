#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy/EnemyBase.h"
#include "JunkerEnemy.generated.h"

class UNiagaraSystem;
class UAnimMontage;
class USoundBase;

/** Junker의 현재 단계 */
UENUM(BlueprintType)
enum class EJunkerPhase : uint8
{
	Ranged  UMETA(DisplayName = "원거리 연발"),
	Rush    UMETA(DisplayName = "돌격"),
	Melee   UMETA(DisplayName = "근접 연타"),
	Retreat UMETA(DisplayName = "후퇴 사격")
};

/**
 * 깡통변기: 원거리와 근접을 번갈아 쓰는 몬스터. 평소에는 다른 몬스터처럼 정찰하다가 플레이어를 발견하면 아래 순서를 반복한다.
 *
 *  1) 원거리(Ranged)  : 4점사를 2~3번 쏜다.
 *  2) 돌격(Rush)      : 일정 시간 이동 속도를 올려 플레이어에게 달려간다.
 *  3) 근접(Melee)     : 근접 범위에 들어오면 주먹질을 여러 번 한다.
 *  4) 후퇴(Retreat)   : 뒤로 물러나면서 4점사를 2~3번 쏜다. 끝나면 다시 돌격으로 이어진다.
 *
 * 돌격 시간 안에 닿지 못하면 원거리 단계로 돌아간다. 같은 BT_Enemy를 쓴다.
 * 돌격 단계에서는 근접 범위에 들어왔을 때만 공격(CanAttackTarget)이 시작되므로 BT가 추격 분기로 이동시킨다.
 */
UCLASS()
class THELASTMESSAGE_API AJunkerEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AJunkerEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual void StartAttack() override;
	virtual void CancelAttack() override;
	virtual bool CanAttackTarget() const override;

	/** 공격 단계 도중에 시야에서 놓쳐도 바로 단계를 초기화하지 않고, 돌격이 끝날 때까지 계속 추적한다. */
	virtual void OnPlayerLost() override;
	virtual void OnPlayerDetected(AActor* Player) override;

	/** 플레이어가 죽으면 도중 추적 없이 바로 정찰로 돌아간다. */
	virtual void OnTargetDied() override;

	UFUNCTION(BlueprintPure, Category = "Junker")
	EJunkerPhase GetPhase() const { return Phase; }

	/** 돌격 중인지(애님 블루프린트에서 돌격 애니메이션 전환에 사용) */
	UFUNCTION(BlueprintPure, Category = "Junker")
	bool IsRushing() const { return Phase == EJunkerPhase::Rush; }

	/** 후퇴 사격 중인지(애님 블루프린트에서 뒷걸음질 사격 전환에 사용) */
	UFUNCTION(BlueprintPure, Category = "Junker")
	bool IsRetreating() const { return Phase == EJunkerPhase::Retreat; }

protected:
	virtual void ApplyEnemyData(const UEnemyDataAsset* Data) override;

	virtual float GetAttackCooldown() const override { return RangedCooldown; }

	// ---- 원거리 ----

	/** 발사할 투사체 블루프린트. 속도·수명은 데이터 에셋에서 정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Ranged")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	/** 투사체 설정(클래스는 위 ProjectileClass, 속도·수명은 데이터 에셋에서 복사된다) */
	FEnemyProjectileConfig MachineGunConfig;

	/** 한 번의 점사에서 쏘는 발수 */
	int32 ShotsPerBurst = 4;

	/** 점사 안에서 발사 간격(초) */
	float FireInterval = 0.12f;

	/** 점사와 점사 사이의 간격(초). 마지막 점사 뒤에도 이만큼 기다린 뒤 다음 단계로 넘어간다. */
	float BurstGap = 0.6f;

	/** 마지막 발사 후 사격 몽타주를 끊고 다음 단계(돌격)로 넘어가기까지의 대기 시간(초) */
	float EndDelay = 0.1f;

	/** 사격 몽타주를 끊을 때의 블렌드 아웃 시간(초). 짧을수록 바로 달리기 자세로 넘어간다. */
	float ShootMontageBlendOut = 0.15f;

	/** 한 단계에서 쏘는 점사 횟수의 최솟값·최댓값(매번 무작위) */
	int32 MinBursts = 2;

	int32 MaxBursts = 3;

	/** 원거리 단계에서 다음 공격 시작까지의 쿨다운(초). 돌격 제한 시간이 끝나 원거리로 돌아왔을 때 적용된다. */
	float RangedCooldown = 1.f;

	/** 발사할 때 재생할 소리. 총구 위치에서 재생된다. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Sound")
	TObjectPtr<USoundBase> FireSound;

	/** 총구 화염 이펙트(한 발마다 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Ranged")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** 투사체·총구 화염이 나갈 스켈레탈 메시 소켓. 없으면 MuzzleOffset을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Ranged")
	FName MuzzleSocketName = TEXT("Fire");

	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	/** 제자리에서 점사할 때 재생할 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Ranged")
	TObjectPtr<UAnimMontage> BurstMontage;

	/** 뒤로 물러나며 점사할 때 재생할 몽타주(뒷걸음질 + 사격). 점사마다 재생된다. */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Retreat")
	TObjectPtr<UAnimMontage> RetreatShootMontage;

	// ---- 돌격 ----

	/** 돌격이 유지되는 최대 시간(초). 이 안에 근접 범위에 닿지 못하면 원거리 단계로 돌아간다. */
	float RushDuration = 4.f;

	/** 돌격 중 이 시간(초) 동안 거의 움직이지 못하고 근접 범위에도 못 들어오면 돌격을 포기하고 원거리 단계로 돌아간다. */
	float RushStallTimeLimit = 0.6f;

	/** 돌격 중 이동 속도 배율(기본 이동 속도 기준) */
	float RushSpeedMultiplier = 1.5f;

	// ---- 근접 ----

	/**
	 * 돌격 중 플레이어와 몸 가장자리(캡슐 표면) 사이 거리가 이 값 안이면 근접 연타를 시작한다.
	 * 추격이 멈추는 거리(Chase Acceptable Radius)보다 커야 한다. 작으면 추격이 멈춘 뒤에도 근접이 시작되지 않는다.
	 */
	float MeleeRange = 200.f;

	/** 한 번의 근접 단계에서 주먹질하는 횟수 */
	int32 MeleeComboCount = 3;

	/** 주먹질 시작부터 다음 주먹질 시작까지의 간격(초) */
	float MeleeInterval = 0.7f;

	/** 주먹질 시작부터 판정까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	float MeleeHitDelay = 0.3f;

	/** 판정 구체의 중심까지 거리(몬스터 앞쪽)와 반경 */
	float MeleeReach = 120.f;

	float MeleeRadius = 120.f;

	/** 주먹질 몽타주(주먹질마다 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Melee")
	TObjectPtr<UAnimMontage> MeleeMontage;

	/**
	 * 주먹질 몽타주 안에서 주먹질 순서(1번째, 2번째, ...)마다 재생할 섹션 이름.
	 * 예: Punch1, Punch2, Punch3. 몽타주의 Sections에 같은 이름의 섹션을 만들어 두면 순서대로 재생된다.
	 * 비워 두거나 이름이 몽타주에 없으면 매번 몽타주 처음부터 재생한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Melee")
	TArray<FName> MeleeSectionNames;

	/** 주먹이 닿았을 때 재생할 이펙트 */
	UPROPERTY(EditDefaultsOnly, Category = "Junker|Melee")
	TObjectPtr<UNiagaraSystem> MeleeHitEffect;

	// ---- 후퇴 ----

	/** 후퇴 사격 중 뒤로 물러나는 최대 거리. 이 거리만큼 물러나면 그 자리에서 사격만 계속한다. */
	float RetreatMaxDistance = 600.f;

	/** 후퇴 중 몸 앞쪽(물러나는 방향) 바닥을 확인하는 거리. 캡슐 가장자리에서 이만큼 앞에 바닥이 없으면 더 물러나지 않는다. */
	float RetreatLedgeCheckDistance = 80.f;

	/** 후퇴 중 앞쪽 바닥이 발밑보다 이 값 이상 낮으면 낭떠러지로 보고 물러나지 않는다. */
	float RetreatMaxDropHeight = 100.f;

	/** 후퇴 사격 중 뒤로 이동하는 속도 */
	float RetreatSpeed = 300.f;

private:
	/** 점사 단계를 시작한다. bInRetreat가 true면 뒤로 물러나며 쏜다. */
	void StartRangedSequence(bool bInRetreat);
	void StartBurst();
	void FireShot();
	void FinishRangedSequence();

	/** 재생 중인 사격 몽타주를 끊는다(다음 단계로 넘어갈 때 남은 모션이 이어지지 않게). */
	void StopShootMontage();

	void StartRush();
	void EndRush();
	void EndRushByTimeout();
	void CheckRushStall();

	/** 플레이어와의 몸 가장자리(캡슐 표면) 사이 거리 */
	float GetEdgeDistanceToTarget() const;
	void StopRushSpeed();

	void StartMeleeCombo();
	void StartPunch();
	void DoMeleeHit();
	void NextPunch();

	void StartRetreat();
	void StopRetreatMovement();

	/** 물러나는 방향 앞쪽에 밟을 바닥이 있는지 확인한다. */
	bool HasGroundBehind(const FVector& AwayDirection) const;

	/** 모든 단계를 처음(원거리 단계)으로 되돌리고 이동 설정과 타이머를 정리한다. */
	void ResetPhase();

	void PauseAI();
	void ResumeAI();

	EJunkerPhase Phase = EJunkerPhase::Ranged;

	/** 지금 진행 중인 점사 단계가 후퇴 사격인지 */
	bool bRetreating = false;
	bool bRetreatMovementActive = false;
	float RetreatStartX = 0.f;

	/** 공격 단계 도중 플레이어를 시야에서 놓쳤고, 아직 놓친 것으로 처리하지 않은 상태 */
	bool bLostWhileEngaged = false;
	bool bSavedCanWalkOffLedges = true;

	/** 지금 재생 중인 사격 몽타주(제자리 또는 후퇴) */
	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveShootMontage;

	int32 BurstsLeft = 0;
	int32 ShotsLeft = 0;

	/** 이번 점사의 첫 발을 쏠 때 정한 조준 방향. 점사 내내 이 방향으로 쏜다(점사마다 새로 정한다). */
	FVector LockedAimDirection = FVector::ZeroVector;
	bool bAimLocked = false;
	int32 PunchesLeft = 0;

	FTimerHandle SequenceTimerHandle;
	FTimerHandle MeleeHitTimerHandle;
	FTimerHandle RushTimerHandle;
	FTimerHandle RushCheckTimerHandle;

	/** 돌격 중 정지 상태로 지난 시간 */
	float RushStallElapsed = 0.f;
};
