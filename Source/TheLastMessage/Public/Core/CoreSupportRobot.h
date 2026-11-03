#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoreSupportRobot.generated.h"

class APlayerBase;

/**
 * 지원 로봇 코어가 만드는 로봇이다. 능력이 전달한 전투 상태와 노릴 적을 보관한다.
 * 매 프레임 목표점으로 한 걸음씩 이동하며, 지형은 구체 Sweep으로 확인해 통과하지 않는다.
 * 평소에는 플레이어 어깨 뒤 위쪽을 따라다니고, 전투 중에는 정해진 경유지를 차례로 따라간다.
 * 화면은 옆(Y축)에서 보는 카메라를 전제로 X·Z 평면 사각형으로 계산한다.
 */
UCLASS()
class THELASTMESSAGE_API ACoreSupportRobot : public AActor
{
	GENERATED_BODY()

public:
	ACoreSupportRobot();

	virtual void Tick(float DeltaSeconds) override;

	/** 지연 생성 중 호출한다. 따라다닐 플레이어를 정한다. */
	bool Initialize(APlayerBase* InPlayer);

	/** 전투 상태를 지금부터 Duration초 동안 유지한다. 이미 더 길게 남아 있으면 줄이지 않는다. */
	void RefreshCombat(float Duration);

	/** 플레이어가 마지막으로 공격한 적을 노릴 대상으로 정한다. */
	void SetTarget(AActor* InTarget);

	/** 전투 상태와 노릴 적, 이동 경로를 모두 비운다. */
	void ClearCombat();

	/** 경유지 하나로 된 경로를 정한다. */
	void SetMoveGoal(const FVector& Goal);

	/** 경유지를 차례로 따라가는 경로를 정한다. 전투가 끝나면 무시하고 플레이어를 따라간다. */
	void SetMovePath(const TArray<FVector>& Waypoints);
	void ClearMoveGoal();

	/** 플레이어 곁(따라다니는 위치)으로 바로 옮긴다. 그 자리가 막혀 있으면 플레이어 위치로 옮긴다. */
	void TeleportToPlayer();

	bool IsInCombat() const;
	AActor* GetTarget() const;
	APlayerBase* GetPlayer() const;

	/** 지금 플레이어 카메라에 보이는 플레이어 깊이 평면의 사각형(X·Z)을 구한다. 카메라가 없으면 false. */
	bool GetScreenBounds(FBox2D& OutBounds) const;

	/** 카메라 위치·방향·가로 시야각·화면비로 PlaneY 평면에 보이는 사각형(X·Z)을 계산한다. */
	static bool CalcScreenBounds(const FVector& CameraLocation, const FRotator& CameraRotation,
		float HorizontalFOV, float AspectRatio, float PlaneY, FBox2D& OutBounds);

protected:
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

	/** 화면 가장자리에서 이만큼 안쪽까지만 목표점을 둔다(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.0"))
	float ScreenMargin = 100.0f;

	/** 로봇이 화면 밖에 이 시간(초) 이상 있으면 전투를 끝내고 플레이어에게 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot|Move", meta = (ClampMin = "0.0"))
	float OffscreenReturnTime = 1.0f;

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

	/** 경유지에 닿으면 다음 경유지로 넘긴다. 마지막 경유지에서는 그 자리에 떠 있는다. */
	void AdvancePath();

	/** 움직이려 했는데 거의 못 움직인 시간이 쌓이면 끼인 것으로 보고 순간이동한다. */
	void UpdateStuck(float DeltaSeconds, float WantedDistance, float MovedDistance);
	void UpdateOffscreen(float DeltaSeconds, const FBox2D* ScreenBounds);

	bool HasPath() const;

	// ---- 지형 판정 ----

	/** 반지름 Radius인 구를 밀어 지형에 막히는지 본다. 플레이어·적 같은 캐릭터 판정은 무시한다. */
	bool SweepTerrain(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit) const;

	/** 로봇 몸이 그 자리에 들어갈 수 있는지(지형과 겹치지 않는지) 본다. */
	bool IsSpotFree(const FVector& Location) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerBase> Player;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Target;

	/** 전투 상태가 끝나는 월드 시간(초). */
	double CombatEndTime = 0.0;

	/** 따라갈 경유지들과 지금 향하는 경유지 번호. 마지막 경유지에 도착하면 그 자리에 떠 있는다. */
	TArray<FVector> Path;
	int32 PathIndex = 0;

	FVector Velocity = FVector::ZeroVector;
	float StuckElapsed = 0.0f;
	float OffscreenElapsed = 0.0f;
};
