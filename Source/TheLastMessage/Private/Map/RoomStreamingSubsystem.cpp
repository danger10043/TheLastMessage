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
#include "Engine/GameInstance.h"
#include "UObject/Package.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Save/ProgressSaveGame.h"

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

namespace
{
	/** 시작 단계에서 리전이나 방을 못 찾았을 때 포기하는 시간(초). */
	constexpr double StartupGiveUpSeconds = 8.0;
	/** 시작 단계 정리를 기다리는 최대 시간(초). */
	constexpr double StartupMaxWaitSeconds = 25.0;
}

FName URoomStreamingSubsystem::ResolveStartRegion() const
{
	const UWorld* World = GetWorld();
	if (const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr)
	{
		if (const UProgressSaveSubsystem* Saves = GameInstance->GetSubsystem<UProgressSaveSubsystem>())
		{
			const UProgressSaveGame* Progress = Saves->GetWorkingProgress();
			// 이어하기: 저장된 체크포인트가 이 맵에 있으면 그 리전부터 올린다.
			if (Saves->GetActiveSlot() > 0 && Progress && !Progress->CheckpointRegionId.IsNone()
				&& Progress->MapPackage == UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()))
			{
				return Progress->CheckpointRegionId;
			}
		}
	}
	return GetDefault<URoomStreamingSettings>()->StartRegionId;
}

void URoomStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 방 레벨 인스턴스는 부모 레벨이 붙을 때 로드를 요청하므로, 필요 없는 방은 로드가 시작되기 전에 그 요청을 취소한다.
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &URoomStreamingSubsystem::HandleLevelAdded);

	// 플레이어가 어느 방에 있는지는 방 안의 리전 액터로 알 수 있는데, 방이 올라오기 전에는 그 액터가 없다.
	// 그래서 시작할 때만 저장된 체크포인트(또는 설정)의 리전으로 올릴 방을 정해 둔다.
	const FName Start = ResolveStartRegion();
	TSet<FName> StartRooms;
	if (!Start.IsNone() && BuildDesiredRooms(Start, StartRooms))
	{
		CurrentRegionId = Start;
		DesiredRooms = MoveTemp(StartRooms);
		bHasDesired = true;
		UE_LOG(LogRoomStreaming, Log, TEXT("Start region %s -> rooms %s"), *Start.ToString(),
			*FString::JoinBy(DesiredRooms, TEXT(", "), [](const FName& Name) { return Name.ToString(); }));
	}
	else if (!Start.IsNone())
	{
		UE_LOG(LogRoomStreaming, Warning, TEXT("시작 리전 %s이(가) 방/복도 설정에 없습니다. 처음에 모든 방이 올라옵니다."), *Start.ToString());
	}
}

void URoomStreamingSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	Super::Deinitialize();
}

void URoomStreamingSubsystem::RegisterBoundActors(ULevel* Level)
{
	if (!Level) return;
	const FString Prefix = RoomBoundTagPrefix();
	bool bAdded = false;
	for (AActor* Actor : Level->Actors)
	{
		if (!Actor) continue;
		FName RoomId;
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(Prefix))
			{
				RoomId = FName(*TagString.Mid(Prefix.Len()));
				break;
			}
		}
		if (RoomId.IsNone()) continue;
		if (BoundActors.ContainsByPredicate([Actor](const FRoomBoundActor& B) { return B.Actor.Get() == Actor; })) continue;

		FRoomBoundActor& Bound = BoundActors.AddDefaulted_GetRef();
		Bound.Actor = Actor;
		Bound.RoomId = RoomId;
		Bound.bOrigHidden = Actor->IsHidden();
		Bound.bOrigCollision = Actor->GetActorEnableCollision();
		Bound.bOrigTick = Actor->IsActorTickEnabled();
		bAdded = true;
	}
	if (bAdded) ApplyBoundActors();
}

void URoomStreamingSubsystem::ApplyBoundActors()
{
	if (BoundActors.IsEmpty()) return;
	UWorld* World = GetWorld();
	ULevelInstanceSubsystem* LevelInstances = World ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	if (!LevelInstances) return;

	TMap<FName, ALevelInstance*> Index;
	BuildLevelInstanceIndex(Index);
	TMap<FName, bool> RoomLoaded;
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
	{
		ALevelInstance** Found = Index.Find(FName(*Room.Level.GetLongPackageName()));
		const ILevelInstanceInterface* Instance = Found ? Cast<ILevelInstanceInterface>(*Found) : nullptr;
		RoomLoaded.Add(Room.RoomId, Instance && LevelInstances->IsLoaded(Instance));
	}

	for (int32 i = BoundActors.Num() - 1; i >= 0; --i)
	{
		FRoomBoundActor& Bound = BoundActors[i];
		AActor* Actor = Bound.Actor.Get();
		if (!Actor || Actor->IsActorBeingDestroyed())
		{
			BoundActors.RemoveAtSwap(i);
			continue;
		}

		// 설정에 없는 방은 관리하지 않으므로 그대로 둔다. 다른 액터에 붙어 있으면(들고 있는 아이템) 방과 상관없이 켠다.
		const bool* bLoaded = RoomLoaded.Find(Bound.RoomId);
		const bool bWantEnabled = !bLoaded || *bLoaded || Actor->GetAttachParentActor() != nullptr;
		if (Bound.bEnabled == bWantEnabled) continue;
		Bound.bEnabled = bWantEnabled;

		Actor->SetActorHiddenInGame(bWantEnabled ? Bound.bOrigHidden : true);
		Actor->SetActorEnableCollision(bWantEnabled ? Bound.bOrigCollision : false);
		Actor->SetActorTickEnabled(bWantEnabled ? Bound.bOrigTick : false);
		UE_LOG(LogRoomStreaming, Verbose, TEXT("Room-bound %s (room %s): %s"), *Actor->GetName(), *Bound.RoomId.ToString(),
			bWantEnabled ? TEXT("enabled") : TEXT("disabled"));
	}
}

void URoomStreamingSubsystem::HandleLevelAdded(ULevel* Level, UWorld* InWorld)
{
	if (Level && InWorld == GetWorld()) RegisterBoundActors(Level);

	// 올릴 방을 아직 모르면(시작 리전이 없을 때) 예전처럼 모두 올라오게 둔다.
	if (!Level || InWorld != GetWorld() || !bHasDesired) return;
	ULevelInstanceSubsystem* LevelInstances = InWorld->GetSubsystem<ULevelInstanceSubsystem>();
	if (!LevelInstances) return;

	TSet<FName> Wanted = DesiredRooms;
	for (const TPair<FName, int32>& Pin : PinnedRooms) Wanted.Add(Pin.Key);

	const URoomStreamingSettings* Settings = GetDefault<URoomStreamingSettings>();
	for (AActor* Actor : Level->Actors)
	{
		ALevelInstance* Instance = Cast<ALevelInstance>(Actor);
		if (!Instance) continue;
		const FName Package(*Instance->GetWorldAsset().GetLongPackageName());
		for (const FRoomStreamEntry& Room : Settings->Rooms)
		{
			if (FName(*Room.Level.GetLongPackageName()) != Package) continue;
			if (!Wanted.Contains(Room.RoomId))
			{
				// 아직 로드가 시작되지 않은 요청만 취소한다(대기열에서 빠질 뿐이라 안전하다).
				// 이미 읽는 중이거나 올라온 방은 Update가 비동기 로딩이 끝난 뒤에 내린다. 읽는 도중에 끊으면 독립형에서 크래시가 난다.
				if (LevelInstances->IsLoading(Instance) || LevelInstances->IsLoaded(Instance)) break;
				LevelInstances->RequestUnloadLevelInstance(Instance);
				UE_LOG(LogRoomStreaming, Verbose, TEXT("Skip initial load of room %s"), *Room.RoomId.ToString());
			}
			break;
		}
	}
}

void URoomStreamingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// 이미 붙어 있는 레벨(LV_Main 등)의 방 종속 액터를 등록한다. 나중에 붙는 레벨은 HandleLevelAdded가 등록한다.
	for (ULevel* Level : InWorld.GetLevels()) RegisterBoundActors(Level);
	if (GetDefault<URoomStreamingSettings>()->Rooms.IsEmpty()) return;
	// 레벨 인스턴스가 없는 맵(메인 메뉴 등)에서는 정리할 것이 없다.
	TActorIterator<ALevelInstance> It(&InWorld);
	if (!It) return;
	bStartupPhase = true;
	StartupBeginTime = FPlatformTime::Seconds();
}

bool URoomStreamingSubsystem::IsStartupSettled() const
{
	if (!bStartupPhase) return true;
	const double Elapsed = FPlatformTime::Seconds() - StartupBeginTime;
	if (Elapsed >= StartupMaxWaitSeconds) return true;

	UWorld* World = GetWorld();
	ULevelInstanceSubsystem* LevelInstances = World ? World->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
	if (!LevelInstances) return true;

	// 플레이어가 있는 리전이 아직 반영되지 않았으면(체크포인트 복원 직후 등) 기다린다.
	const FName Region = FindRegionAtPlayer();
	if (!Region.IsNone() && Region != CurrentRegionId)
	{
		TSet<FName> Unused;
		if (BuildDesiredRooms(Region, Unused)) return false;
	}
	if (!bHasDesired) return Elapsed >= StartupGiveUpSeconds;

	TSet<FName> Wanted = DesiredRooms;
	for (const TPair<FName, int32>& Pin : PinnedRooms) Wanted.Add(Pin.Key);

	TMap<FName, ALevelInstance*> Index;
	BuildLevelInstanceIndex(Index);
	bool bFoundAny = false;
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
	{
		ALevelInstance** Found = Index.Find(FName(*Room.Level.GetLongPackageName()));
		const ILevelInstanceInterface* Instance = Found ? Cast<ILevelInstanceInterface>(*Found) : nullptr;
		if (!Instance)
		{
			// 필요한 방의 부모 레벨이 아직 안 올라왔을 수 있다. 잠시 기다린 뒤 이 맵에 없는 방으로 본다.
			if (Wanted.Contains(Room.RoomId) && Elapsed < StartupGiveUpSeconds) return false;
			continue;
		}
		bFoundAny = true;
		if (Wanted.Contains(Room.RoomId))
		{
			if (!LevelInstances->IsLoaded(Instance)) return false;
		}
		else if (LevelInstances->IsLoaded(Instance) || LevelInstances->IsLoading(Instance))
		{
			return false; // 아직 내려가는 중이다.
		}
	}
	return bFoundAny || Elapsed >= StartupGiveUpSeconds;
}

TStatId URoomStreamingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoomStreamingSubsystem, STATGROUP_Tickables);
}

void URoomStreamingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Accumulated += DeltaTime;
	// 시작 단계에서는 로딩 화면 뒤라서 더 자주 확인해 빨리 정리한다.
	const float Interval = bStartupPhase ? 0.1f : GetDefault<URoomStreamingSettings>()->UpdateIntervalSeconds;
	if (Accumulated < Interval) return;
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

	ApplyBoundActors();

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

		const bool bIsLoading = LevelInstances->IsLoading(Instance);
		const bool bLoaded = LevelInstances->IsLoaded(Instance) || bIsLoading;
		if (!bLoaded || Wanted.Contains(Room.RoomId)) UnloadRequestedAt.Remove(Room.RoomId);
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
			// 로드 중인 방에 언로드를 요청하면 패키지가 로드되는 도중에 지워진다(느린 PC·독립형 게임에서 크래시). 다 올라온 뒤에 내린다.
			if (bIsLoading) continue;
			// 다른 패키지가 비동기로 읽히는 중에 레벨을 내리면 독립형에서 비동기 로더가 크래시한다(스택: ProcessAsyncLoading).
			// 로딩이 모두 끝난 뒤에 내린다. 시작 단계에서는 로딩 화면이 그 시간을 가려 준다.
			if (IsAsyncLoading()) continue;
			// 한 번 요청한 방은 내려가는 동안 다시 요청하지 않는다. 오래 지나도 안 내려가면 다시 요청한다.
			if (const double* RequestedAt = UnloadRequestedAt.Find(Room.RoomId))
			{
				if (Now - *RequestedAt < 5.0) continue;
			}
			// 시작 단계에서는 로딩 화면 뒤에서 처리하므로 대기 없이 바로 내린다.
			const double Delay = bStartupPhase ? 0.0 : Settings->UnloadDelaySeconds;
			const double Deadline = UnloadAfter.FindOrAdd(Room.RoomId, Now + Delay);
			if (Now >= Deadline)
			{
				UE_LOG(LogRoomStreaming, Log, TEXT("Unload room %s"), *Room.RoomId.ToString());
				UnloadAfter.Remove(Room.RoomId);
				UnloadRequestedAt.Add(Room.RoomId, Now);
				LevelInstances->RequestUnloadLevelInstance(Instance);
			}
		}
	}
}

bool URoomStreamingSubsystem::PrepareRegionForRespawn(FName RegionId, const FVector& Location, const FVector& Gravity)
{
	UWorld* World = GetWorld();
	if (!World || RegionId.IsNone()) return true;
	TSet<FName> Rooms;
	if (!BuildDesiredRooms(RegionId, Rooms)) return true; // 설정에 없는 리전: 올릴 방이 없다.

	const double Now = World->GetTimeSeconds();
	const bool bNewHold = HoldRegionId != RegionId || Now >= HoldExpireTime;
	HoldRegionId = RegionId;
	HoldExpireTime = Now + 30.0;
	Update(); // 기다리지 않고 바로 로드를 요청한다.

	if (bNewHold)
	{
		LoadedSince = -1.0;
		bHoldNeededLoading = !AreDesiredRoomsLoaded();
	}
	if (!AreDesiredRoomsLoaded())
	{
		LoadedSince = -1.0;
		return false;
	}
	if (LoadedSince < 0.0) LoadedSince = Now;
	// 방을 새로 올린 경우에만 충돌과 바닥이 자리 잡을 시간을 더 준다.
	const double Settle = bHoldNeededLoading ? GetDefault<URoomStreamingSettings>()->RespawnSettleSeconds : 0.0;
	if (Now - LoadedSince < Settle) return false;

	// 부활 위치에 실제로 바닥 충돌이 있는지 확인한다. 로드는 끝났어도 충돌이 아직 없으면 플레이어가 빈 곳으로 떨어진다.
	const FVector Down = Gravity.GetSafeNormal(SMALL_NUMBER, FVector(0, 0, -1));
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	return World->LineTraceSingleByObjectType(Hit, Location - Down * 100.0, Location + Down * 800.0, Objects);
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

bool URoomStreamingSubsystem::IsRegionStreamedIn(FName RegionId) const
{
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
	{
		if (Room.RegionIds.Contains(RegionId)) return IsRoomLoaded(Room.RoomId);
	}
	return true;
}

void URoomStreamingSubsystem::DumpState() const
{
	UE_LOG(LogRoomStreaming, Display, TEXT("Current region: %s"), *CurrentRegionId.ToString());
	for (const FRoomStreamEntry& Room : GetDefault<URoomStreamingSettings>()->Rooms)
		UE_LOG(LogRoomStreaming, Display, TEXT("  Room %s: wanted=%d loaded=%d"), *Room.RoomId.ToString(),
			DesiredRooms.Contains(Room.RoomId), IsRoomLoaded(Room.RoomId));
}
