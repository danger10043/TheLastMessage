#include "Character/Enemy/Spawn/EnemySpawnSubsystem.h"
#include "Character/Enemy/Spawn/EnemySpawnPoint.h"
#include "Character/Player/PlayerBase.h"
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

void UEnemySpawnSubsystem::Deinitialize()
{
	if (APlayerBase* Player = BoundPlayer.Get())
	{
		Player->OnPlayerRespawned.RemoveDynamic(this, &UEnemySpawnSubsystem::HandlePlayerRespawned);
	}
	BoundPlayer.Reset();

	Super::Deinitialize();
}

void UEnemySpawnSubsystem::BindPlayerEvents()
{
	APlayerBase* Player = Cast<APlayerBase>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || BoundPlayer.Get() == Player)
	{
		return;
	}

	if (APlayerBase* OldPlayer = BoundPlayer.Get())
	{
		OldPlayer->OnPlayerRespawned.RemoveDynamic(this, &UEnemySpawnSubsystem::HandlePlayerRespawned);
	}

	Player->OnPlayerRespawned.AddUniqueDynamic(this, &UEnemySpawnSubsystem::HandlePlayerRespawned);
	BoundPlayer = Player;
}

void UEnemySpawnSubsystem::HandlePlayerRespawned(APlayerBase* Player)
{
	// 부활 직후에는 플레이어 위치가 아직 체크포인트로 옮겨지기 전일 수 있다.
	// 모든 몬스터를 먼저 회수하고, 다시 스폰하는 것은 다음 Tick에서 부활 위치를 보고 한다.
	const TArray<TWeakObjectPtr<AEnemySpawnPoint>> PointsCopy = SpawnPoints;
	for (const TWeakObjectPtr<AEnemySpawnPoint>& Point : PointsCopy)
	{
		if (AEnemySpawnPoint* SpawnPoint = Point.Get())
		{
			SpawnPoint->ResetForPlayerRespawn();
		}
	}

	bForceStreamingCheck = true;
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

	// 플레이어는 월드가 시작된 뒤에 생기므로, 연결되지 않았다면 매 프레임 가볍게 확인한다.
	BindPlayerEvents();

	TimeSinceCheck += DeltaTime;
	if (TimeSinceCheck < CheckInterval && !bForceStreamingCheck)
	{
		return;
	}
	TimeSinceCheck = 0.f;
	bForceStreamingCheck = false;

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
