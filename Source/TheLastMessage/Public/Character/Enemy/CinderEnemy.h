#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Character/Enemy/EnemyBase.h"
#include "CinderEnemy.generated.h"

class UNiagaraSystem;
class UAnimMontage;
class UMaterialInterface;
class USoundBase;

/** Cinder의 현재 이동 방식 */
UENUM(BlueprintType)
enum class ECinderMode : uint8
{
	Crawling UMETA(DisplayName = "표면 이동"),
	Falling  UMETA(DisplayName = "낙하 중"),
	Fallen   UMETA(DisplayName = "착지 후 자폭 돌진")
};

/**
 * 포 벌레: 벽·천장·바닥을 기어다니며 원거리 공격을 하다가, 체력이 일정 비율 이하가 되면 바닥으로 떨어져 플레이어에게 빠르게 달려가 자폭한다.
 *
 * - 표면 이동은 NavMesh를 쓰지 않는다. 표면에 선을 쏴서(Trace) 표면을 따라가고, 모서리에서는 면을 바꿔 타고 넘는다.
 * - 정찰·추격 이동은 이 클래스의 Tick이 직접 한다. BT는 공격 시작(StartAttack)에만 쓰인다.
 * - 스폰된 위치에서 가장 가까운 표면에 자동으로 붙는다. 스폰 포인트를 표면 가까이(SpawnSnapDistance 이내)에 두면 된다.
 */
UCLASS()
class THELASTMESSAGE_API ACinderEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ACinderEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual void StartAttack() override;
	virtual void CancelAttack() override;
	virtual bool CanAttackTarget() const override;
	virtual void ResetEnemy() override;
	virtual void HandleDeath() override;
	virtual void Landed(const FHitResult& Hit) override;

	/**
	 * 표면을 기어가는 동안은 이동 컴포넌트 대신 직접 계산한 속도를 돌려준다.
	 * BT의 이동 노드가 이동을 멈추라는 요청을 보내 컴포넌트 속도가 0이 되어도 ABP의 Get Velocity가 이동 애니메이션을 계속 재생한다.
	 */
	virtual FVector GetVelocity() const override;

	UFUNCTION(BlueprintPure, Category = "Cinder")
	ECinderMode GetCinderMode() const { return Mode; }

	/** 표면을 따라 이동하는 현재 속도(애님 블루프린트의 이동 애니메이션 전환에 사용). 멈춰 있으면 0. */
	UFUNCTION(BlueprintPure, Category = "Cinder|Animation")
	float GetCrawlSpeed() const { return CrawlSpeed; }

	/** 낙하 후 자폭 돌진 중인지(애님 블루프린트에서 달리기 자세 전환에 사용) */
	UFUNCTION(BlueprintPure, Category = "Cinder")
	bool HasFallen() const { return Mode == ECinderMode::Fallen; }

protected:
	virtual void ApplyEnemyData(const UEnemyDataAsset* Data) override;

	virtual void BeginPlay() override;

	// ---- 표면 이동 ----

	/** 몸 중심이 표면에서 떨어져 있는 거리. 캡슐/메시 크기에 맞춰 조정한다. */
	float SurfaceOffset = 60.f;

	/** 표면을 찾을 때 SurfaceOffset 외에 더 확인하는 거리 */
	float SurfaceSnapDistance = 40.f;

	/** 앞쪽 장애물(오목한 모서리)을 미리 확인하는 거리 */
	float CornerCheckDistance = 40.f;

	/** 스폰될 때 가장 가까운 표면을 찾는 최대 거리 */
	float SpawnSnapDistance = 300.f;

	/** 켜면 움직이는 물체(월드 다이나믹)도 기어갈 표면으로 인식한다. 끄면 고정된 지형(월드 스태틱)만 인식한다. */
	bool bUseWorldDynamicSurfaces = false;

	/** 몸이 표면 방향으로 돌아가는 속도(클수록 빠름, 0이면 즉시) */
	float RotationInterpSpeed = 12.f;

	/** 모서리를 돌 때 몸이 위치를 따라가는 속도(클수록 빠름, 0이면 즉시). 낮추면 모서리에서 부드럽게 넘어간다. */
	float PositionInterpSpeed = 15.f;

	// ---- 정찰 ----

	/** 한 번 이동하는 거리의 최솟값·최댓값 */
	float PatrolMinDistance = 300.f;

	float PatrolMaxDistance = 800.f;

	/** 이동 사이에 멈춰 있는 시간의 최솟값·최댓값(초) */
	float PatrolWaitMin = 1.f;

	float PatrolWaitMax = 2.5f;

	// ---- 원거리 ----

	/** 발사할 투사체 블루프린트. 속도·수명은 데이터 에셋에서 정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Ranged")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	/** 투사체 설정(클래스는 위 ProjectileClass, 속도·수명은 데이터 에셋에서 복사된다) */
	FEnemyProjectileConfig ProjectileConfig;

	/** 공격 시작부터 발사까지의 지연(초). 애님 노티파이 도입 전까지 사용하는 임시 값. */
	float FireDelay = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Ranged")
	FName MuzzleSocketName = TEXT("Fire");

	FVector MuzzleOffset = FVector(80.f, 0.f, 30.f);

	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Ranged")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	// ---- 낙하·자폭 ----

	/** 체력 비율이 이 값 이하가 되면 바닥으로 떨어진다 (0.5 = 50%) */
	float FallHealthRatio = 0.5f;

	/** 착지 후 플레이어에게 달려가는 속도 */
	float FallenMoveSpeed = 700.f;

	/** 플레이어와 몸 가장자리 사이 거리가 이 값 이하가 되면 자폭 준비를 시작한다. */
	float ExplosionTriggerRange = 150.f;

	/** 자폭 준비를 시작하고 폭발할 때까지의 시간(초). 이 동안 멈춰서 예고한다. */
	float ExplosionFuseTime = 0.4f;

	float ExplosionRadius = 250.f;

	/** 자폭 피해에 붙일 피해 유형 태그(예: Damage.Type.Explosion). 비워 두면 붙이지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Explosion")
	FGameplayTag ExplosionDamageTag;

	/**
	 * 자폭 준비 몽타주. 공격(사격) 몽타주와 별개로 쓴다. 비워 두면 재생하지 않는다.
	 * 반동이나 루트 모션이 있는 몽타주는 몸이 뒤로 밀리는 것처럼 보일 수 있으니 제자리에서 부풀거나 떠는 모션을 권장한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Explosion")
	TObjectPtr<UAnimMontage> FuseMontage;

	/**
	 * 자폭 준비 중 몸에 덧씌워 깜빡일 오버레이 머티리얼(예: 빨간색). 비워 두면 깜빡이지 않는다.
	 * 피격 번쩍임(Hit Flash Material)과 같은 방식의 오버레이 머티리얼이며, 메시 사용 플래그 Used with Skeletal Mesh가 켜져 있어야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Explosion")
	TObjectPtr<UMaterialInterface> FuseBlinkMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	/** 폭발 이펙트 크기 배율. 이펙트가 너무 크거나 작으면 이 값으로 조절한다(1 = 원래 크기). */
	float ExplosionEffectScale = 1.f;

	/**
	 * 자폭하러 가다가 플레이어를 놓치면(시야에서 사라지거나 플레이어가 죽으면) 그 자리에서 자폭 준비를 시작하고
	 * 이 시간(초) 뒤에 폭발한다. 그동안 몸이 깜빡인다.
	 */
	float LostTargetFuseTime = 2.f;

	/**
	 * 자폭 준비 중 몸이 깜빡여 켜질 때마다 재생할 삐 소리(짧은 한 번짜리). 깜빡임이 빨라지면 소리도 같이 빨라진다.
	 * 비워 두면 재생하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Sound")
	TObjectPtr<USoundBase> FuseBeepSound;

	/** 투사체를 발사할 때 재생할 소리. 총구 위치에서 재생된다. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Sound")
	TObjectPtr<USoundBase> FireSound;

	/** 폭발할 때 재생할 소리. 처치당해서 터질 때도 같은 소리가 난다. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Cinder|Sound")
	TObjectPtr<USoundBase> ExplosionSound;

	/** 삐 소리의 피치. 자폭 준비 시작 때 값에서 폭발 직전 값까지 점점 올라간다(1 = 원래 높이). */
	float FuseBeepPitchStart = 1.f;
	float FuseBeepPitchEnd = 1.4f;

	/** 자폭 준비를 시작할 때의 깜빡임 간격(초). 켜짐/꺼짐이 바뀌는 간격이다. */
	float FuseBlinkStartInterval = 0.2f;

	/** 폭발 직전의 깜빡임 간격(초). 시작 간격에서 이 값까지 남은 시간에 따라 점점 빨라진다. */
	float FuseBlinkEndInterval = 0.04f;

private:
	/** 체력이 줄 때마다 낙하 조건을 확인한다. */
	UFUNCTION()
	void HandleHealthChanged(float Damage);

	void TickCrawl(float DeltaTime);
	void TickFallen(float DeltaTime);

	/** 표면을 따라 한 걸음 이동한다. 실제로 이동한 거리를 반환한다. */
	float CrawlStep(float DeltaTime, float Speed);

	/** 표면 위의 이동 방향(단위 벡터). Sign이 +1/-1이면 반대 방향이다. */
	FVector GetTangent(float Sign) const;

	/** 이동 방향이 플레이어를 향하도록 MoveSign을 정한다. */
	void FaceMoveDirectionToward(const FVector& WorldPoint);

	/** 현재 표면과 이동 방향에 맞게 몸을 돌리고, 위치를 따라가게 한다. */
	void ApplyCrawlTransform(float DeltaTime);

	bool TraceSurface(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	static FVector ProjectNormal(const FVector& Normal);

	/** 스폰 위치에서 가장 가까운 표면에 붙는다. */
	void SnapToSurface();
	void ResetCinderState();
	void BeginPatrolWait();

	void StartFall();
	void EnterPhysicsMovement(EMovementMode NewMode);

	void FireProjectile();

	/** 자폭 준비를 시작한다. FuseDuration 뒤에 폭발하고, 그동안 몸이 깜빡인다. */
	void StartFuse(float FuseDuration);

	/** 자폭까지 남은 시간이 줄어들수록 빠르게 몸을 깜빡인다. */
	void UpdateFuseBlink(float DeltaTime);
	void SetFuseBlink(bool bOn);

	/** 자폭 준비가 끝났을 때: 스스로 사망 처리한다. 폭발은 사망 처리(HandleDeath)에서 일어난다. */
	void Explode();

	/** 그 자리에서 폭발한다: 범위 안 플레이어에게 피해, 이펙트, 몸 숨김. 한 번만 실행된다. */
	void Detonate();

	/** 플레이어와 몸 가장자리(캡슐 표면) 사이 거리 */
	float GetEdgeDistanceToTarget() const;

	ECinderMode Mode = ECinderMode::Crawling;

	FVector SurfaceNormal = FVector::UpVector;
	FVector CrawlPosition = FVector::ZeroVector;
	float MoveSign = 1.f;
	float LaneY = 0.f;

	float PatrolRemaining = 0.f;
	float WaitRemaining = 0.f;

	float FallGravityScale = 1.f;

	/** 표면을 따라 이동하는 속도. 이동 컴포넌트의 Velocity에도 넣어 일반적인 속도 기반 ABP가 그대로 동작하게 한다. */
	float CrawlSpeed = 0.f;

	/** 자폭 준비 때 폰 충돌을 겹침으로 바꾸기 전의 원래 설정 */
	TEnumAsByte<ECollisionResponse> DefaultPawnResponse = ECR_Block;
	bool bFuseStarted = false;
	bool bFuseBlinkOn = false;
	float FuseElapsed = 0.f;
	float ActiveFuseTime = 0.4f; // 지금 진행 중인 자폭 준비의 길이(초)
	float FuseBlinkTimer = 0.f;
	bool bExploded = false;

	FTimerHandle FireTimerHandle;
	FTimerHandle FuseTimerHandle;
};
