#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyHealthPickup.generated.h"

class APlayerBase;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

/**
 * 몬스터가 죽으면 일정 확률로 떨어지는 체력 회복 아이템.
 *
 * - 죽은 자리에 떠다니며(위아래, 회전) 머물러 있다.
 * - 플레이어가 흡수 범위(Attract Radius) 안에 들어오면 플레이어 쪽으로 점점 빨라지며 빨려 들어가고,
 *   도착하면 체력을 회복한다.
 * - 회복과 흡수 이동은 서버에서만 처리한다. 대기 중의 위아래·회전은 메시 컴포넌트만 움직이는 연출이라 모든 클라이언트에서 돈다.
 * - 외형(메시), 소리, 이펙트는 이 클래스를 상속한 블루프린트에서 지정한다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AEnemyHealthPickup : public AActor
{
	GENERATED_BODY()

public:
	AEnemyHealthPickup();

	/** 스폰 직후(FinishSpawning 전)에 회복량을 정한다. */
	void InitPickup(float InHealAmount);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** 흡수 이동이 시작될 때(블루프린트에서 소리·이펙트를 넣을 때 사용) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup", meta = (DisplayName = "On Absorb Started"))
	void BP_OnAbsorbStarted();

	/** 플레이어에게 도착해 체력을 회복한 직후(제거되기 전) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup", meta = (DisplayName = "On Collected"))
	void BP_OnCollected(APlayerBase* Player);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<USceneComponent> Root;

	/** 외형 메시. 블루프린트에서 Static Mesh와 머티리얼을 지정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup|Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** 위아래로 떠다니는 높이(cm). 0이면 움직이지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Idle", meta = (ClampMin = "0.0"))
	float BobHeight = 10.f;

	/** 1초에 위아래로 왕복하는 횟수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Idle", meta = (ClampMin = "0.0"))
	float BobSpeed = 0.8f;

	/** 초당 회전 각도(도). 0이면 회전하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Idle")
	float SpinSpeed = 90.f;

	/** 이 시간(초)이 지나면 먹지 않아도 사라진다. 0이면 사라지지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Idle", meta = (ClampMin = "0.0"))
	float LifeSeconds = 20.f;

	/** 플레이어가 이 거리 안에 들어오면 빨려 들어가기 시작한다(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb", meta = (ClampMin = "0.0"))
	float AttractRadius = 250.f;

	/** 빨려 들어가기 시작할 때의 속도(cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb", meta = (ClampMin = "0.0"))
	float AbsorbStartSpeed = 200.f;

	/** 최고 속도(cm/s). AbsorbAccelTime에 걸쳐 시작 속도에서 이 속도까지 점점 빨라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb", meta = (ClampMin = "0.0"))
	float AbsorbMaxSpeed = 1800.f;

	/** 시작 속도에서 최고 속도까지 가속하는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb", meta = (ClampMin = "0.01"))
	float AbsorbAccelTime = 0.4f;

	/** 플레이어와 이 거리 안으로 들어오면 도착한 것으로 보고 회복한다(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb", meta = (ClampMin = "0.0"))
	float ArriveDistance = 40.f;

	/** 플레이어 위치에서 흡수 도착 지점까지의 오프셋. 몸 중심보다 위아래로 옮길 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb")
	FVector AbsorbTargetOffset = FVector::ZeroVector;

	/** 도착할 때의 크기 비율(흡수되면서 작아진다). 1이면 크기가 변하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Absorb", meta = (ClampMin = "0.0"))
	float AbsorbEndScale = 0.3f;

	/** 회복할 때 재생할 소리. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Effects")
	TObjectPtr<USoundBase> CollectSound;

	/** 회복할 때 플레이어 위치에서 재생할 이펙트. 비워 두면 재생하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Effects")
	TObjectPtr<UNiagaraSystem> CollectEffect;

	/** 이 아이템이 회복시킬 체력. 몬스터가 떨어뜨릴 때 정해진다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Pickup")
	float HealAmount = 0.f;

private:
	/** 대기 중 위아래·회전 연출(모든 머신) */
	void UpdateIdle(float DeltaSeconds);

	/** 흡수 범위 안에서 가장 가까운 살아 있는 플레이어를 찾는다(서버). */
	APlayerBase* FindAttractTarget() const;

	void StartAbsorb(APlayerBase* Player);
	void UpdateAbsorb(float DeltaSeconds);

	/** 플레이어를 회복시키고 아이템을 제거한다(서버). */
	void Collect(APlayerBase* Player);

	float IdleTime = 0.f;
	float DetectTimer = 0.f;
	float AbsorbElapsed = 0.f;
	bool bAbsorbing = false;
	bool bCollected = false;
	FVector BaseMeshScale = FVector::OneVector;
	TWeakObjectPtr<APlayerBase> AbsorbTarget;
};
