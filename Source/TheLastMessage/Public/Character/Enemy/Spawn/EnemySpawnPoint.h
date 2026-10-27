#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/Enemy/EnemyBase.h"
#include "EnemySpawnPoint.generated.h"

class UBoxComponent;
class UArrowComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpawnedEnemyDiedSignature, AEnemyBase*, Enemy);

/** 리셋 반경을 벗어났을 때(또는 ResetSpawn 호출 때) 몬스터를 어떻게 다시 스폰할지 */
UENUM(BlueprintType)
enum class EEnemyResetMode : uint8
{
	RespawnAll       UMETA(DisplayName = "모두 제거 후 새로 스폰"),
	RespawnDeadOnly  UMETA(DisplayName = "죽은 몬스터만 다시 스폰"),
	None             UMETA(DisplayName = "리셋하지 않음")
};

/** 스폰할 몬스터 한 종류의 설정 */
USTRUCT(BlueprintType)
struct FEnemySpawnEntry
{
	GENERATED_BODY()

	/** 스폰할 몬스터 클래스 (예: BP_Rattler_Test) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AEnemyBase> EnemyClass;

	/** 스폰할 마릿수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 Count = 1;

	/** 몬스터 레벨. 후반 지역일수록 높게 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 Level = 1;
};

/**
 * 맵에 배치하는 몬스터 스폰 구역. 지정한 몬스터들을 범위 안에 겹치지 않게 무작위로 스폰한다.
 * Y는 스폰 포인트의 Y로 고정하고, X(좌우)와 선택적으로 Z(높이)만 범위 안에서 무작위로 정한다.
 */
UCLASS()
class THELASTMESSAGE_API AEnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawnPoint();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 설정한 몬스터들을 스폰한다. 이미 스폰돼 있으면 아무것도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Spawn")
	void SpawnEnemies();

	/** 이 구역의 몬스터를 모두 풀로 돌려보낸다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Spawn")
	void DespawnEnemies();

	/** 리셋: Reset Mode에 따라 몬스터를 다시 스폰한다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Spawn")
	void ResetSpawn();

	/**
	 * 플레이어가 체크포인트에서 부활했을 때 호출한다. Reset Mode와 상관없이 이 구역의 몬스터를 모두 풀로 돌려보낸다.
	 * 거리 기반 구역은 서브시스템이 부활 위치 기준으로 다시 스폰하고, 거리 기반이 아닌 구역은 바로 새로 스폰한다.
	 */
	void ResetForPlayerRespawn();

	/** 지금 살아 있는 몬스터 수 (구역 클리어 판정 등에 사용) */
	UFUNCTION(BlueprintPure, Category = "Enemy|Spawn")
	int32 GetAliveEnemyCount() const;

	/**
	 * 플레이어 위치를 보고 스폰·해제·리셋을 판단한다. (EnemySpawnSubsystem이 일정 간격으로 호출)
	 * - 스폰 반경 안으로 들어오면 스폰
	 * - 해제 반경 밖으로 나가면 모두 풀로 회수
	 * - 리셋 반경 안에 들어왔다가 밖으로 나가면 몬스터를 리셋(죽은 몬스터 부활, 추격 중이던 몬스터 원위치)
	 */
	void UpdateStreaming(const FVector& PlayerLocation);

	/** 이 구역의 몬스터가 죽었을 때(처치 수 집계 등에 사용) */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Spawn")
	FOnSpawnedEnemyDiedSignature OnSpawnedEnemyDied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleSpawnedEnemyDied(AEnemyBase* Enemy);

	/** 몬스터가 풀로 돌아갔을 때(시체 제거·강제 제거) 슬롯의 참조를 비운다. */
	UFUNCTION()
	void HandleSpawnedEnemyReleased(AEnemyBase* Enemy);

	/** 스폰할 몬스터 목록 (종류별 마릿수·레벨) */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TArray<FEnemySpawnEntry> Entries;

	/** 스폰 위치를 흩뿌릴 좌우(X) 범위. 스폰 포인트를 중심으로 ± 이 값. Y는 스폰 포인트 값으로 고정된다. */
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0.0"))
	float SpawnRangeX = 500.f;

	/** 높이(Z) 무작위 범위(±). 0이면 스폰 포인트의 높이로 고정된다. */
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0.0"))
	float SpawnRangeZ = 0.f;

	/** 몬스터끼리 최소 간격(X 방향). 범위가 좁아 지킬 수 없으면 가능한 만큼만 벌린다. */
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0.0"))
	float MinSpacing = 150.f;

	/** 켜면 플레이어와의 거리로 스폰·해제·리셋을 관리한다. 끄면 게임 시작 시 바로 스폰한다. */
	UPROPERTY(EditAnywhere, Category = "Spawn|Streaming")
	bool bStreamByDistance = true;

	/** 플레이어가 이 거리 안에 들어오면 스폰한다. (스폰 포인트 기준, 높이(Z)와 좌우(X)만 계산) */
	UPROPERTY(EditAnywhere, Category = "Spawn|Streaming", meta = (EditCondition = "bStreamByDistance", ClampMin = "0.0"))
	float SpawnRadius = 4000.f;

	/** 플레이어가 이 거리 밖으로 나가면 몬스터를 모두 회수한다. 스폰 반경보다 크게 두면 경계에서 깜빡이지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Spawn|Streaming", meta = (EditCondition = "bStreamByDistance", ClampMin = "0.0"))
	float DespawnRadius = 4500.f;

	/** 플레이어가 이 거리 안에 들어왔다가 밖으로 나가면 몬스터를 리셋한다. 스폰 범위(SpawnRangeX)와 화면 폭보다 크게 둔다. */
	UPROPERTY(EditAnywhere, Category = "Spawn|Streaming", meta = (EditCondition = "bStreamByDistance", ClampMin = "0.0"))
	float ResetRadius = 2000.f;

	/** 리셋 반경 리셋 때의 동작. 플레이어가 체크포인트에서 부활할 때는 이 설정과 상관없이 모든 몬스터를 새로 스폰한다. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	EEnemyResetMode ResetMode = EEnemyResetMode::RespawnAll;

	/** 스폰 범위 표시(에디터에서만 보임) */
	UPROPERTY(VisibleAnywhere, Category = "Spawn")
	TObjectPtr<UBoxComponent> RangeBox;

	UPROPERTY(VisibleAnywhere, Category = "Spawn")
	TObjectPtr<UArrowComponent> DirectionArrow;

private:
	/** 몬스터 한 마리의 스폰 정보 */
	struct FSpawnSlot
	{
		TSubclassOf<AEnemyBase> EnemyClass;
		int32 Level = 1;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<AEnemyBase> Enemy;
		bool bSpawned = false;
	};

	/** 몬스터 목록과 범위로 스폰 슬롯(위치 포함)을 새로 만든다. */
	void BuildSlots();

	void SpawnSlot(FSpawnSlot& Slot);

	/** 슬롯의 몬스터와의 연결을 끊고 풀로 돌려보낸다. */
	void ReleaseSlotEnemy(FSpawnSlot& Slot);

	/** 플레이어가 리셋 반경 안에 들어온 적이 있다(밖으로 나가면 리셋). */
	bool bResetArmed = false;

	TArray<FSpawnSlot> Slots;
};
