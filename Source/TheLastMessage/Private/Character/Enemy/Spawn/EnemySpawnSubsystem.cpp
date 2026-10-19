#include "Character/Enemy/Spawn/EnemySpawnSubsystem.h"
#include "Character/Enemy/Spawn/EnemySpawnPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

void UEnemySpawnSubsystem::RegisterSpawnPoint(AEnemySpawnPoint* SpawnPoint)
{
	if (SpawnPoint)
	{
		SpawnPoints.AddUnique(SpawnPoint);
	}
}

void UEnemySpawnSubsystem::UnregisterSpawnPoint(AEnemySpawnPoint* SpawnPoint)
{
	SpawnPoints.Remove(SpawnPoint);
}

void UEnemySpawnSubsystem::RespawnAllEnemies()
{
	// 리셋 중 목록이 바뀔 수 있으므로 복사본으로 순회한다.
	const TArray<TWeakObjectPtr<AEnemySpawnPoint>> PointsCopy = SpawnPoints;
	for (const TWeakObjectPtr<AEnemySpawnPoint>& Point : PointsCopy)
	{
		if (AEnemySpawnPoint* SpawnPoint = Point.Get())
		{
			SpawnPoint->ResetSpawn();
		}
	}
}

bool UEnemySpawnSubsystem::IsTickable() const
{
	// 에디터 미리보기 월드나 CDO에서는 동작하지 않는다.
	return !IsTemplate() && GetWorld() && GetWorld()->IsGameWorld();
}

TStatId UEnemySpawnSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemySpawnSubsystem, STATGROUP_Tickables);
}

void UEnemySpawnSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceCheck += DeltaTime;
	if (TimeSinceCheck < CheckInterval)
	{
		return;
	}
	TimeSinceCheck = 0.f;

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	// 스폰·해제·리셋 중 목록이 바뀔 수 있으므로 복사본으로 순회한다.
	const TArray<TWeakObjectPtr<AEnemySpawnPoint>> PointsCopy = SpawnPoints;
	for (const TWeakObjectPtr<AEnemySpawnPoint>& Point : PointsCopy)
	{
		if (AEnemySpawnPoint* SpawnPoint = Point.Get())
		{
			SpawnPoint->UpdateStreaming(PlayerLocation);
		}
	}
}
