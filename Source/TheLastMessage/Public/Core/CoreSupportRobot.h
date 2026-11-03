#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "CoreSupportRobot.generated.h"

class ACoreSupportRobotProjectile;
class APlayerBase;
class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundBase;
struct FCollisionQueryParams;

/**
 * 지원 로봇 코어가 만드는 로봇이다. 생성·파괴와 전투 신호 전달은 능력(UGA_CoreSupportRobot)이 맡고,
 * 로봇은 "누구를 노릴지 → 어디서 쏠지 → 어떻게 갈지"를 스스로 정해 움직인다.
 *
 * [매 프레임]
 *   1. 전투 판단이 필요하면(1초마다, 또는 대상 변경·전투 시작·도착 직후) RunDecision을 실행한다.
 *   2. 경로가 있으면 다음 경유지, 없으면 플레이어 어깨 뒤를 목표로 한 걸음 이동한다(지형은 Sweep으로 막힘).
 *
 * [전투 판단 RunDecision]
 *   대상 선택 → 예측 조준점 계산 → 지금 자리에서 쏠 수 있으면 그 자리에 멈춤
 *   → 못 쏘면 새 자리 찾기(선호 지점 → 안 되면 화면 격자 BFS) → 자리가 없으면 그 적을 잠시 제외하고 다음 대상.
 *   처음 노리는 적이면 쏠 수 있어도 먼저 공격 위치로 날아가고(교전 시작 이동), 도착하면 쏜다.
 *   전투 중에는 몸을 노리는 적 쪽으로 돌리고 위아래로 기울여 조준하는 모습을 보인다.
 *
 * [사격]
 *   멈춰 쏠 수 있다고 판단될 때 쿨타임(1초)이 끝났으면 총구에서 예측 조준점으로 전용 탄을 쏜다.
 *   피해는 플레이어의 SupportRobotDamage 속성(코어 단계별 4/6/8)이며 플레이어의 피해 배율·치명타는 붙지 않는다.
 *   피해의 공격자는 로봇이고, 피해 함수가 공격자에서 ASC를 찾으므로 로봇은 플레이어 ASC를 돌려준다.
 *   연출(총구 섬광·발사음·사격 몽타주)은 몬스터처럼 C++ 값으로 지정하고 C++에서 재생한다. 대기·이동 애니메이션은 메시의 ABP가 맡는다.
 *
 * 횡스크롤 게임이라 모든 위치 계산은 플레이어 깊이(Y)의 X·Z 평면에서 한다.
 * 로봇은 충돌을 끄고, 지형 확인은 직접 Sweep으로 한다(다른 판정에 로봇이 걸리지 않게).
 */
UCLASS()
class THELASTMESSAGE_API ACoreSupportRobot : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACoreSupportRobot();

	virtual void Tick(float DeltaSeconds) override;

	/** 플레이어의 ASC를 돌려준다. 로봇을 공격자로 피해를 줄 때 피해 함수가 이 ASC로 효과를 만든다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 지연 생성 중 호출한다. 따라다닐 플레이어를 정한다. */
	bool Initialize(APlayerBase* InPlayer);

	/** 전투 상태를 지금부터 Duration초 동안 유지한다. 이미 더 길게 남아 있으면 줄이지 않는다. */
	void RefreshCombat(float Duration);

	/** 전투 상태와 노릴 적, 이동 경로를 모두 비운다. */
	void ClearCombat();

	/**
	 * 플레이어가 공격한 적을 알린다. 가장 우선하는 대상이다.
	 * 다시 때렸다는 건 플레이어가 그 적을 원한다는 뜻이라 제외 목록에서도 뺀다.
	 */
	void SetPlayerHitTarget(AActor* Enemy);

	/** 플레이어를 때린 적을 알린다. 플레이어가 때린 적이 없을 때의 대상이다. 적이 아니면 무시한다. */
	void SetAttackerTarget(AActor* Enemy);

	/** 죽은 적처럼 더 노리면 안 되는 대상을 잊는다. */
	void ForgetTarget(const AActor* Enemy);

	/** 지금 노리는 적. 전투 판단에서 정해진다. */
	AActor* GetTarget() const;

	/** 경유지 하나로 된 경로를 정한다. */
	void SetMoveGoal(const FVector& Goal);

	/** 경유지를 차례로 따라가는 경로를 정한다. 전투가 끝나면 무시하고 플레이어를 따라간다. */
	void SetMovePath(const TArray<FVector>& Waypoints);
	void ClearMoveGoal();

	/** 플레이어 곁(따라다니는 위치)으로 바로 옮긴다. 그 자리가 막혀 있으면 플레이어 위치로 옮긴다. */
	void TeleportToPlayer();

	bool IsInCombat() const;
	APlayerBase* GetPlayer() const;

	/** 지금 플레이어 카메라에 보이는 플레이어 깊이 평면의 사각형(X·Z)을 구한다. 카메라가 없으면 false. */
	bool GetScreenBounds(FBox2D& OutBounds) const;

	/** 카메라 위치·방향·가로 시야각·화면비로 PlaneY 평면에 보이는 사각형(X·Z)을 계산한다. */
	static bool CalcScreenBounds(const FVector& CameraLocation, const FRotator& CameraRotation,
		float HorizontalFOV, float AspectRatio, float PlaneY, FBox2D& OutBounds);

	/**
	 * 탄이 날아가는 동안 대상이 움직일 거리만큼 앞을 노린 조준점을 구한다.
	 * 대상이 지금 속도를 유지한다고 가정하며, 너무 먼 앞을 노리지 않게 MaxLeadTime(초)까지만 예측한다.
	 */
	static FVector PredictAimPoint(const FVector& ShooterLocation, const FVector& TargetLocation,
		const FVector& TargetVelocity, float InProjectileSpeed, float InMaxLeadTime);

	/** 살아 있는 일반 몬스터나 보스인지 확인한다. 기믹·플레이어 등은 대상이 될 수 없다. */
	static bool IsAliveEnemy(const AActor* Actor);

	/**
	 * 실제 이동 속도(cm/s)를 돌려준다. 로봇은 이동 컴포넌트 없이 직접 움직여 기본 구현이 항상 0을 돌려주므로,
	 * 애니메이션(ABP)이 대기·이동을 구분할 수 있게 이동 계산에 쓰는 속도를 그대로 준다.
	 */
	virtual FVector GetVelocity() const override;

	/** 탄이 나오는 위치. 메시에 총구 소켓(뼈 이름도 됨)이 있으면 그 위치, 없으면 액터 기준 MuzzleOffset 위치다. */
	UFUNCTION(BlueprintPure, Category = "Core|SupportRobot|Combat")
	FVector GetMuzzleLocation() const;

protected:
	/** 로봇 모습. 충돌은 없고 보이기·애니메이션만 맡는다. BP에서 메시·머티리얼·ABP를 지정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|SupportRobot")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** 총구로 쓸 메시의 소켓 또는 뼈 이름. 몬스터의 MuzzleSocketName과 같은 방식이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat")
	FName MuzzleSocketName = TEXT("Minigun");

	/** 메시에 총구 소켓이 없을 때 쓰는 액터 기준 위치(cm). X가 로봇이 보는 방향이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat")
	FVector MuzzleOffset = FVector(40.0f, 0.0f, 0.0f);

	/** 쏠 때 총구(MuzzleSocketName)에 붙여 재생할 섬광. 몬스터의 MuzzleFlashEffect와 같은 방식이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Effect")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** 쏠 때 총구 위치에서 재생할 소리. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Effect")
	TObjectPtr<USoundBase> FireSound;

	/** 쏠 때 재생할 사격 몽타주. 메시의 ABP에 몽타주 슬롯(DefaultSlot)이 있어야 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Effect")
	TObjectPtr<UAnimMontage> FireMontage;

	/** 지형 확인에 쓰는 로봇 몸 반지름(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "1.0"))
	float CollisionRadius = 30.0f;

	/** 평소 따라다니는 위치. X는 플레이어가 바라보는 반대쪽, Y는 중력 반대쪽으로 적용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move")
	FVector2D FollowOffset = FVector2D(80.0f, 150.0f);

	/** 최대 이동 속도(cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "1.0"))
	float MaxSpeed = 800.0f;

	/** 목표까지 이 거리 안이면 감속한다(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "1.0"))
	float SlowdownRadius = 200.0f;

	/** 원하는 속도로 맞춰 가는 빠르기. 클수록 방향 전환이 급하다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.1"))
	float SteeringRate = 6.0f;

	/** 이 거리 안이면 경유지에 도착한 것으로 본다(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "1.0"))
	float ArriveDistance = 30.0f;

	/** 움직이려는데 이 시간(초) 동안 거의 못 움직이면 플레이어 곁으로 순간이동한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.1"))
	float StuckTime = 1.5f;

	/** 화면 가장자리에서 이만큼 안쪽까지만 목표점·탐색 노드를 둔다(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.0"))
	float ScreenMargin = 100.0f;

	/** 로봇이 화면 밖에 이 시간(초) 이상 있으면 전투를 끝내고 플레이어에게 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.0"))
	float OffscreenReturnTime = 1.0f;

	/** 몸이 목표 방향으로 돌아가는 빠르기. 클수록 빨리 돈다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.1"))
	float TurnSpeed = 10.0f;

	/** 적을 조준할 때 위아래로 기울이는 최대 각도(도). 너무 크면 드론이 뒤집혀 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxAimPitch = 45.0f;

	/** 전투 판단(대상·자리 확인)을 반복하는 주기(초). 공격 주기와 같다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.1"))
	float DecisionInterval = 1.0f;

	/** 쏘는 탄. 모양·이펙트를 붙인 BP를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat")
	TSubclassOf<ACoreSupportRobotProjectile> ProjectileClass;

	/** 피해를 줄 때 쓰는 효과. 플레이어 무기와 같은 IncomingDamageExecution 효과를 지정한다. 없으면 쏘지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 한 발을 쏜 뒤 다음 발까지의 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.1"))
	float FireInterval = 1.0f;

	/** 이 거리 안이고 가리는 것이 없으면 그 자리에서 쏜다(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "1.0"))
	float FireRange = 1200.0f;

	/**
	 * 새 자리를 찾을 때 조준점과의 거리 띠(cm). 이 띠 안의 자리만 1·2등급 후보가 된다.
	 * 쏘는 중에는 FireRange까지 허용하므로, 띠 바깥 9~12m는 "이미 있으면 머무르고 일부러 오지는 않는" 여유 구간이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.0"))
	float GoalMinDistance = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "1.0"))
	float GoalMaxDistance = 900.0f;

	/** 선호 지점. 조준점에서 X는 플레이어 쪽으로, Y는 월드 위쪽으로 떨어진 거리(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat")
	FVector2D PreferredOffset = FVector2D(600.0f, 300.0f);

	/** 조준점보다 이만큼 이상 높은 자리를 1등급으로 친다(cm). 내려다보며 쏘는 드론다운 자리다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.0"))
	float AboveTargetHeight = 150.0f;

	/** 탄 속도(cm/s). 예측 조준에 쓴다. 플레이어 플라즈마 피스톨과 같다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "1.0"))
	float ProjectileSpeed = 4500.0f;

	/** 탄 반지름(cm). 시야 판정 Sweep에 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.0"))
	float ProjectileRadius = 10.0f;

	/** 예측 조준이 내다보는 최대 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.0"))
	float MaxLeadTime = 1.0f;

	/** 쏠 자리를 못 찾은 적을 대상에서 빼 두는 시간(초). 플레이어가 다시 때리면 바로 풀린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Combat", meta = (ClampMin = "0.0"))
	float ExcludeDuration = 3.0f;

	/** 자리 탐색 격자 간격(cm). 작을수록 촘촘하지만 확인할 노드가 늘어난다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Search", meta = (ClampMin = "10.0"))
	float GridSpacing = 150.0f;

	/** 로봇 위치에서 바로 연결해 보는 주변 노드 범위(칸). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Search", meta = (ClampMin = "1"))
	int32 StartLinkCells = 2;

	/** 한 번의 탐색에서 확인할 최대 노드 수. 넓은 화면에서 한 프레임이 너무 무거워지지 않게 막는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Search", meta = (ClampMin = "1"))
	int32 MaxSearchNodes = 300;

	/** 카메라를 알 수 없을 때 플레이어 중심으로 대신 쓰는 화면 크기(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Search")
	FVector2D FallbackViewSize = FVector2D(2600.0f, 1400.0f);

private:
	/** 능력이 끝나지 않은 채 플레이어가 사라져도 로봇이 남지 않게 한다. */
	UFUNCTION()
	void HandlePlayerDestroyed(AActor* DestroyedActor);

	// ---- 이동 ----

	/** 플레이어 어깨 뒤 위쪽. 플레이어가 바라보는 방향과 중력 방향을 따른다. */
	FVector GetFollowPoint() const;

	/** 이번 프레임에 향할 목표점. 화면 안으로 자르고 깊이는 플레이어 평면에 맞춘다. */
	FVector GetCurrentGoal(const FBox2D* ScreenBounds) const;

	/** Delta만큼 Sweep으로 이동하고 실제로 움직인 양을 돌려준다. 막히면 표면을 따라 한 번 미끄러진다. */
	FVector MoveWithSweep(const FVector& Delta);

	/** 경유지에 닿으면 다음 경유지로 넘기고, 마지막 경유지에 닿으면 바로 전투 판단을 요청한다. */
	void AdvancePath();

	/** 움직이려 했는데 거의 못 움직인 시간이 쌓이면 끼인 것으로 보고 순간이동한다. */
	void UpdateStuck(float DeltaSeconds, float WantedDistance, float MovedDistance);
	void UpdateOffscreen(float DeltaSeconds, const FBox2D* ScreenBounds);

	/** 전투 중엔 노리는 적을 향해 좌우·위아래로, 평소엔 이동 방향으로 몸을 돌린다. */
	void UpdateFacing(float DeltaSeconds);

	bool HasPath() const;

	// ---- 지형 판정 ----

	/** 반지름 Radius인 구를 밀어 지형에 막히는지 본다. 플레이어·적 같은 캐릭터 판정은 무시한다. */
	bool SweepTerrain(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const;
	bool IsPathBlocked(const FVector& Start, const FVector& End, float Radius) const;

	/** 로봇 몸이 그 자리에 들어갈 수 있는지(지형과 겹치지 않는지) 본다. */
	bool IsSpotFree(const FVector& Location) const;

	/** 지형 판정에서 빼야 할 액터(플레이어, 로봇이 쏜 탄)를 넣는다. */
	void AddTerrainQueryIgnores(FCollisionQueryParams& Params) const;

	// ---- 전투 판단 ----

	/** 대상 선택부터 경로 결정까지 한 번의 전투 판단. */
	void RunDecision();

	/** 플레이어가 때린 적 → 플레이어를 때린 적 순서로, 살아 있고 제외되지 않은 적을 고른다. */
	AActor* ChooseTarget();

	FVector GetAimPoint(const AActor* Enemy) const;

	/** From에서 쏘면 조준점까지 사거리 안이고 지형에 가리지 않는지. */
	bool CanShootFrom(const FVector& From, const FVector& AimPoint) const;

	/** 그 자리를 이동 목표로 쓸 때의 등급. 1(띠 안·위쪽·보임) > 2(띠 안·보임) > 3(사거리 안·보임), 0은 못 씀. */
	int32 RateSpot(const FVector& Spot, const FVector& AimPoint) const;

	/** 노드와 선호 지점을 둘 탐색 범위(화면에서 여백을 뺀 사각형). */
	FBox2D GetSearchBounds() const;

	FVector GetPreferredPoint(const FVector& AimPoint, const FBox2D& SearchBounds) const;

	/** 쏠 자리까지의 경로를 찾는다. 선호 지점을 먼저 보고, 안 되면 격자 BFS를 쓴다. */
	bool FindAttackPath(const FVector& AimPoint, TArray<FVector>& OutPath) const;

	/** 화면 격자에서 로봇 위치부터 BFS로 퍼지며 가장 좋은 등급의 가장 가까운 자리를 찾는다. */
	bool SearchGridPath(const FVector& AimPoint, const FVector& PreferredPoint, const FBox2D& SearchBounds,
		TArray<FVector>& OutPath) const;

	/** 격자를 따라 꺾인 경로에서, 막히지 않고 건너뛸 수 있는 경유지를 지워 직선 몇 개로 편다. */
	void SmoothPath(TArray<FVector>& InOutPoints) const;

	/** 지금 자리에 멈춰 떠 있는다. */
	void HoldPosition();

	/** 쿨타임이 끝났고 피해가 있으면 총구에서 조준점으로 한 발 쏜다. 쐈으면 true. */
	bool TryFire(const FVector& AimPoint);

	/** 총구 섬광·발사음·사격 몽타주를 재생한다. 지정되지 않은 것은 건너뛴다. */
	void PlayFireEffects(const FVector& MuzzleLocation);

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerBase> Player;

	/** 플레이어가 마지막으로 때린 적, 플레이어를 마지막으로 때린 적, 그중 지금 노리는 적. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PlayerHitTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AttackerTarget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Target;

	/** 쏠 자리를 못 찾아 잠시 빼 둔 적과 풀리는 월드 시간(초). */
	TMap<TWeakObjectPtr<AActor>, double> ExcludedUntil;

	/** 전투 상태가 끝나는 월드 시간(초). */
	double CombatEndTime = 0.0;
	bool bWasInCombat = false;

	/** 따라갈 경유지들과 지금 향하는 경유지 번호. 마지막 경유지에 도착하면 그 자리에 떠 있는다. */
	TArray<FVector> Path;
	int32 PathIndex = 0;
	bool bArrivalHandled = false;

	/** 지난 전투 판단 뒤 흐른 시간과, 다음 프레임에 바로 판단하라는 요청. */
	float DecisionElapsed = 0.0f;
	bool bDecisionRequested = false;

	/** 다음 발을 쏠 수 있을 때까지 남은 시간(초). */
	float FireCooldownRemaining = 0.0f;

	/**
	 * 마지막으로 교전을 시작한 적과, 그 적을 향한 교전 시작 이동 중인지.
	 * 새 적이면 지금 자리에서 쏠 수 있어도 공격 위치로 먼저 날아가고(bEngaging), 도착할 때까지 이동을 끊지 않는다.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> EngagedTarget;

	bool bEngaging = false;

	/**
	 * 쏜 뒤 아직 날아가는 탄. 탄은 지형처럼 WorldDynamic을 막는 충돌이라, 총구에서 막 나온 탄과 로봇이 겹치면
	 * 로봇의 지형 판정이 탄을 벽으로 보고 로봇을 밀어낸다. 그래서 지형 판정에서 이 탄들을 뺀다.
	 */
	TArray<TWeakObjectPtr<AActor>> LiveProjectiles;

	FVector Velocity = FVector::ZeroVector;
	float StuckElapsed = 0.0f;
	float OffscreenElapsed = 0.0f;
};
