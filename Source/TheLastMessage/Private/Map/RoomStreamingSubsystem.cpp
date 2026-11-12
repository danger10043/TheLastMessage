#include "Map/RoomStreamingSubsystem.h"
#include "Map/RoomStreamingSettings.h"
#include "Map/AreaMapRegion.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogRoomStreaming);

namespace
{
	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("Rooms.Dump"),
		TEXT("현재 리전과 방 스트리밍 상태를 로그로 출력한다."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const URoomStreamingSubsystem* Rooms = World ? World->GetSubsystem<URoomStreamingSubsystem>() : nullptr)
				Rooms->DumpState();
			else
				UE_LOG(LogRoomStreaming, Display, TEXT("Room Streaming이 꺼져 있습니다(Project Settings > Room Streaming)."));
		}));
}

bool URoomStreamingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const URoomStreamingSettings* Settings = GetDefault<URoomStreamingSettings>();
	return Settings && Settings->bEnabled && Super::ShouldCreateSubsystem(Outer);
}

bool URoomStreamingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URoomStreamingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoomStreamingSubsystem, STATGROUP_Tickables);
}

void URoomStreamingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Accumulated += DeltaTime;
	if (Accumulated < GetDefault<URoomStreamingSettings>()->UpdateIntervalSeconds) return;
	Accumulated = 0.f;
	Update();
}

FName URoomStreamingSubsystem::FindRegionAtPlayer() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UWorld* World = GetWorld();
	if (!Pawn || !World) return NAME_None;

	const FVector2D Point(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Z);
	FName Best = NAME_None;
	double BestArea = TNumericLimits<double>::Max();
	for (TActorIterator<AAreaMapRegion> It(World); It; ++It)
	{
		if (It->RegionId.IsNone()) continue;
		const FBox2D Bounds = It->GetMapBounds();
		if (!Bounds.IsInside(Point)) continue;
		// 겹치면 더 작은 리전(더 구체적인 방/복도)을 우선한다.
		const FVector2D Size = Bounds.GetSize();
		const double Area = Size.X * Size.Y;
		if (Area < BestArea)
		{
			BestArea = Area;
			Best = It->RegionId;
		}
	}
	return Best;
}

bool URoomStreamingSubsystem::BuildDesiredRooms(FName RegionId, TSet<FName>& OutRooms) const
{
	const URoomStreamingSettings* Settings = GetDefault<URoomStreamingSettings>();

	for (const FRoomStreamEntry& Room : Settings->Rooms)
	{
		if (!Room.RegionIds.Contains(RegionId)) continue;
		OutRooms.Add(Room.RoomId);
		// 복도로 이어진 이웃 방.
		for (const FRoomCorridorEntry& Corridor : Settings->Corridors)
			if (Corridor.RoomIds.Contains(Room.RoomId))
				OutRooms.Append(Corridor.RoomIds);
		return true;
	}
	for (const FRoomCorridorEntry& Corridor : Settings->Corridors)
	{
		if (Corridor.CorridorId != RegionId) continue;
		OutRooms.Append(Corridor.RoomIds);
		return true;
	}
	return false;
}

void URoomStreamingSubsystem::BuildLevelInstanceIndex(TMap<FName, ALevelInstance*>& OutIndex) const
{
	UWorld* World = GetWorld();
	if (!World) return;
	for (TActorIterator<ALevelInstance> It(World); It; ++It)
	{
		const FName Package(*It->GetWorldAsset().GetLongPackageName());
		if (!Package.IsNone()) OutIndex.Add(Package, *It);
	}
}

void URoomStreamingSubsystem::Update()
{
	UWorld* World = GetWorld();
	ULevelInstanceSubsystem* LevelInstances = World ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	if (!LevelInstances) return;

	// 부활 대기 중이면 플레이어 위치 대신 고정된 리전을 따른다. 어느 리전에도 없으면(점프 중, 복도 리전이 아직 없는 구간) 마지막 상태를 유지한다.
	FName Found;
	if (!HoldRegionId.IsNone() && World->GetTimeSeconds() < HoldExpireTime)
	{
		Found = HoldRegionId;
	}
	else
	{
		HoldRegionId = NAME_None;
		Found = FindRegionAtPlayer();
	}
	if (!Found.IsNone() && Found != CurrentRegionId)
	{
		TSet<FName> NewRooms;
		if (BuildDesiredRooms(Found, NewRooms))
		{
			CurrentRegionId = Found;
			DesiredRooms = MoveTemp(NewRooms);
			bHasDesired = true;
			UE_LOG(LogRoomStreaming, Log, TEXT("Region %s -> rooms %s"), *Found.ToString(),
				*FString::JoinBy(DesiredRooms, TEXT(", "), [](const FName& Name) { return Name.ToString(); }));
		}
	}
	// 설정에 없는 리전은 이전 상태를 유지한다.
	if (!bHasDesired && PinnedRooms.IsEmpty()) return;

	TSet<FName> Wanted = DesiredRooms;
	for (const TPair<FName, int32>& Pin : PinnedRooms) Wanted.Add(Pin.Key);

	const URoomStreamingSettings* Settings = GetDefault<URoomStreamingSettings>();
	TMap<FName, ALevelInstance*> Index;
	BuildLevelInstanceIndex(Index);
	const double Now = World->GetTimeSeconds();

	for (const FRoomStreamEntry& Room : Settings->Rooms)
	{
		ALevelInstance** Found2 = Index.Find(FName(*Room.Level.GetLongPackageName()));
		ILevelInstanceInterface* Instance = Found2 ? Cast<ILevelInstanceInterface>(*Found2) : nullptr;
		if (!Instance) continue; // 부모 레벨이 아직 로드되지 않았거나 이 맵에 없는 방.

		const bool bLoaded = LevelInstances->IsLoaded(Instance) || LevelInstances->IsLoading(Instance);
		if (Wanted.Contains(Room.RoomId))
		{
			UnloadAfter.Remove(Room.RoomId);
			if (!bLoaded)
			{
				UE_LOG(LogRoomStreaming, Log, TEXT("Load room %s"), *Room.RoomId.ToString());
				LevelInstances->RequestLoadLevelInstance(Instance, false);
			}
		}
		else if (bLoaded)
		{
			const double* Deadline = UnloadAfter.Find(Room.RoomId);
			if (!Deadline)
			{
				UnloadAfter.Add(Room.RoomId, Now + Settings->UnloadDelaySeconds);
			}
			else if (Now >= *Deadline)
			{
				UE_LOG(LogRoomStreaming, Log, TEXT("Unload room %s"), *Room.RoomId.ToString());
				UnloadAfter.Remove(Room.RoomId);
				LevelInstances->RequestUnloadLevelInstance(Instance);
			}
		}
	}
}

bool URoomStreamingSubsystem::PrepareRegionForRespawn(FName RegionId)
{
	UWorld* World = GetWorld();
	if (!World || RegionId.IsNone()) return true;
	TSet<FName> Rooms;
	if (!BuildDesiredRooms(RegionId, Rooms)) return true; // 설정에 없는 리전: 올릴 방이 없다.

	HoldRegionId = RegionId;
	HoldExpireTime = World->GetTimeSeconds() + 30.0;
	Update(); // 기다리지 않고 바로 로드를 요청한다.
	return AreDesiredRoomsLoaded();
}

bool URoomStreamingSubsystem::AreDesiredRoomsLoaded() const
{
	UWorld* World = GetWorld();
	ULevelInstanceSubsystem* LevelInstances = World ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	if (!LevelInstances) return true;
	TMap<FName, ALevelInstance*> Index;
	BuildLevelInstanceIndex(Index);
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
	{
		if (!DesiredRooms.Contains(Room.RoomId)) continue;
		ALevelInstance** Found = Index.Find(FName(*Room.Level.GetLongPackageName()));
		const ILevelInstanceInterface* Instance = Found ? Cast<ILevelInstanceInterface>(*Found) : nullptr;
		if (Instance && !LevelInstances->IsLoaded(Instance)) return false;
	}
	return true;
}

void URoomStreamingSubsystem::PinRooms(const TArray<FName>& RoomIds)
{
	for (const FName& Room : RoomIds) ++PinnedRooms.FindOrAdd(Room);
	Update(); // 기다리지 않고 바로 로드를 요청한다.
}

void URoomStreamingSubsystem::UnpinRooms(const TArray<FName>& RoomIds)
{
	for (const FName& Room : RoomIds)
	{
		int32* Count = PinnedRooms.Find(Room);
		if (!Count) continue;
		if (--(*Count) <= 0) PinnedRooms.Remove(Room);
	}
}

bool URoomStreamingSubsystem::AreRoomsLoaded(const TArray<FName>& RoomIds) const
{
	UWorld* World = GetWorld();
	ULevelInstanceSubsystem* LevelInstances = World ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	if (!LevelInstances) return true;
	TMap<FName, ALevelInstance*> Index;
	BuildLevelInstanceIndex(Index);
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
	{
		if (!RoomIds.Contains(Room.RoomId)) continue;
		ALevelInstance** Found = Index.Find(FName(*Room.Level.GetLongPackageName()));
		const ILevelInstanceInterface* Instance = Found ? Cast<ILevelInstanceInterface>(*Found) : nullptr;
		if (Instance && !LevelInstances->IsLoaded(Instance)) return false;
	}
	return true;
}

bool URoomStreamingSubsystem::IsRoomLoaded(FName RoomId) const
{
	UWorld* World = GetWorld();
	ULevelInstanceSubsystem* LevelInstances = World ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	if (!LevelInstances) return false;
	TMap<FName, ALevelInstance*> Index;
	BuildLevelInstanceIndex(Index);
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
	{
		if (Room.RoomId != RoomId) continue;
		ALevelInstance** Found = Index.Find(FName(*Room.Level.GetLongPackageName()));
		const ILevelInstanceInterface* Instance = Found ? Cast<ILevelInstanceInterface>(*Found) : nullptr;
		return Instance && LevelInstances->IsLoaded(Instance);
	}
	return false;
}

void URoomStreamingSubsystem::DumpState() const
{
	UE_LOG(LogRoomStreaming, Display, TEXT("Current region: %s"), *CurrentRegionId.ToString());
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
		UE_LOG(LogRoomStreaming, Display, TEXT("  Room %s: wanted=%d loaded=%d"), *Room.RoomId.ToString(),
			DesiredRooms.Contains(Room.RoomId), IsRoomLoaded(Room.RoomId));
}
