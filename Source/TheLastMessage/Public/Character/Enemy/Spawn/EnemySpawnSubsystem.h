#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemySpawnSubsystem.generated.h"

class AEnemySpawnPoint;
class APlayerBase;

/**
 * 월드에 배치된 모든 AEnemySpawnPoint를 모아 두고 관리한다.
 *
 * - 거리 기반 스트리밍: 일정 간격(CheckInterval)마다 플레이어와 각 스폰 포인트의 거리를 확인해
 *   스폰 포인트가 몬스터를 스폰·해제·리셋하도록 알려 준다. (반경은 각 스폰 포인트에서 설정)
 * - 플레이어가 체크포인트에서 부활하면(OnPlayerRespawned) 모든 구역의 몬스터를 풀로 돌려보내고,
 *   부활 위치 기준 스폰 반경 안의 구역만 바로 새로 스폰한다. 나머지 구역은 가까워질 때 스폰된다.
 * - 전체 리셋이 필요하면 RespawnAllEnemies()를 호출한다.
 */
UCLASS()
class THELASTMESSAGE_API UEnemySpawnSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterSpawnPoint(AEnemySpawnPoint* SpawnPoint);
	void UnregisterSpawnPoint(AEnemySpawnPoint* SpawnPoint);

	/** 모든 스폰 포인트를 리셋한다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Spawn")
	void RespawnAllEnemies();

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** 플레이어와의 거리를 확인하는 간격(초). 매 프레임 확인할 필요가 없다. */
	float CheckInterval = 0.2f;

private:
	/** 플레이어가 체크포인트에서 부활했을 때: 모든 구역의 몬스터를 회수하고, 다음 확인에서 부활 위치 기준으로 다시 스폰한다. */
	UFUNCTION()
	void HandlePlayerRespawned(APlayerBase* Player);

	/** 플레이어 폰이 바뀌었거나 아직 연결하지 못했다면 부활 이벤트를 연결한다. */
	void BindPlayerEvents();

	TArray<TWeakObjectPtr<AEnemySpawnPoint>> SpawnPoints;

	TWeakObjectPtr<APlayerBase> BoundPlayer;

	/** 부활 직후라 간격을 기다리지 않고 다음 Tick에서 바로 스트리밍을 확인한다. */
	bool bForceStreamingCheck = false;

	float TimeSinceCheck = 0.f;
};
