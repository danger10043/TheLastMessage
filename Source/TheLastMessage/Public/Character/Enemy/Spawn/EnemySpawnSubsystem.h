#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemySpawnSubsystem.generated.h"

class AEnemySpawnPoint;

/**
 * 월드에 배치된 모든 AEnemySpawnPoint를 모아 두고 관리한다.
 *
 * - 거리 기반 스트리밍: 일정 간격(CheckInterval)마다 플레이어와 각 스폰 포인트의 거리를 확인해
 *   스폰 포인트가 몬스터를 스폰·해제·리셋하도록 알려 준다. (반경은 각 스폰 포인트에서 설정)
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

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** 플레이어와의 거리를 확인하는 간격(초). 매 프레임 확인할 필요가 없다. */
	float CheckInterval = 0.2f;

private:
	TArray<TWeakObjectPtr<AEnemySpawnPoint>> SpawnPoints;

	float TimeSinceCheck = 0.f;
};
