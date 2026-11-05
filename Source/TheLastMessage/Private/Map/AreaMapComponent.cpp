#include "Map/AreaMapComponent.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Map/AreaMapRegion.h"
#include "Components/StaticMeshComponent.h"
#include "Character/Player/PlayerBase.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Checkpoint/PlayerCheckpoint.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"

UAreaMapComponent::UAreaMapComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
}

void UAreaMapComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(FMath::Max(RevealInterval, 0.05f));
}

void UAreaMapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player || !Player->IsLocallyControlled()) return;
	if (!bLoadedExploration) LoadExploration();
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= 2)
	{
		RefreshLayout();
		RefreshElapsed = 0;
	}
	if (!Player->IsDead())
	{
		ExploreAt(Player->GetActorLocation());
		RevealFromView(Player->GetActorLocation());
	}
}

void UAreaMapComponent::RefreshLayout()
{
	Rooms.Reset(); Surfaces.Reset(); Checkpoints.Reset(); MapBounds.Init();
	if (!GetWorld()) return;
	const double PlaneY = GetOwner() ? GetOwner()->GetActorLocation().Y : 0;
	for (TActorIterator<AAreaMapRegion> It(GetWorld()); It; ++It)
	{
		if (It->RegionId.IsNone()) continue;
		FAreaMapRoom& Room = Rooms.AddDefaulted_GetRef();
		Room.Id = It->RegionId; Room.Name = It->RegionName; Room.Bounds = It->GetMapBounds();
		Room.Silhouette = It->MapSilhouette.Get();
		MapBounds += Room.Bounds;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->IsA<APawn>() || Actor->GetOwner() && Actor->GetOwner()->IsA<APawn>()) continue;
		// 체크포인트 클래스 이름 대신 플레이어 체크포인트 기반 타입으로 식별
		if (Actor->IsA<APlayerCheckpoint>())
			Checkpoints.Add(FVector2D(Actor->GetActorLocation().X, Actor->GetActorLocation().Z));
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			if (!Mesh->GetStaticMesh() || !Actor->GetActorEnableCollision() || !Mesh->IsCollisionEnabled()) continue;
			const FName Profile = Mesh->GetCollisionProfileName();
			if (Profile == TEXT("InvisibleWall") || Profile == TEXT("InvisibleWallDynamic")) continue;
			if (Mesh->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
			const FBox Bounds = Mesh->Bounds.GetBox();
			// 플레이 평면에서 벗어난 배경과 안내판은 지형 표시에서 제외
			if (Bounds.Min.Y > PlaneY + 100 || Bounds.Max.Y < PlaneY - 100) continue;
			FBox2D Surface(FVector2D(Bounds.Min.X, Bounds.Min.Z), FVector2D(Bounds.Max.X, Bounds.Max.Z));
			if (Surface.GetSize().X < 1 || Surface.GetSize().Y < 1) continue;
			Surfaces.Add(Surface); MapBounds += Surface;
		}
	}
	// 방 영역이 없는 새 지형에도 자동 탐사 구역 제공
	if (MapBounds.bIsValid)
	{
		const int32 AuthoredRoomCount = Rooms.Num();
		const FVector2D Cell(FMath::Max(100.0, FallbackCellSize.X), FMath::Max(100.0, FallbackCellSize.Y));
		const int32 X0 = FMath::FloorToInt(MapBounds.Min.X / Cell.X), X1 = FMath::FloorToInt(MapBounds.Max.X / Cell.X);
		const int32 Y0 = FMath::FloorToInt(MapBounds.Min.Y / Cell.Y), Y1 = FMath::FloorToInt(MapBounds.Max.Y / Cell.Y);
		if (int64(X1-X0+1) * (Y1-Y0+1) <= 10000)
		for (int32 X = X0; X <= X1; ++X) for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			const FBox2D Bounds(FVector2D(X*Cell.X,Y*Cell.Y), FVector2D((X+1)*Cell.X,(Y+1)*Cell.Y));
			bool bCovered = false;
			for (int32 Index=0;Index<AuthoredRoomCount;++Index)
				if (Rooms[Index].Bounds.Intersect(Bounds)) { bCovered=true; break; }
			if (bCovered) continue;
			// 새 지형 주변만 보완하고 빈 공간은 탐사율 계산에서 제외
			if (!Surfaces.ContainsByPredicate([&](const FBox2D& Surface) { return Surface.Intersect(Bounds); })) continue;
			FAreaMapRoom& Room = Rooms.AddDefaulted_GetRef();
			Room.Id = FName(*FString::Printf(TEXT("Auto_%d_%d"),X,Y));
			Room.Name = NSLOCTEXT("AreaMap", "UnknownArea", "미지의 구역"); Room.Bounds = Bounds;
			Room.bAutomatic = true;
		}
	}
}

bool UAreaMapComponent::ExploreAt(const FVector& Location)
{
	const FVector2D Point(Location.X, Location.Z);
	const FAreaMapRoom* Best = nullptr;
	for (const FAreaMapRoom& Room : Rooms)
	{
		if (!Room.Bounds.IsInside(Point)) continue;
		if (!Best || Room.Id == CurrentRoom || Room.Bounds.GetArea() < Best->Bounds.GetArea()) Best = &Room;
	}
	// 방 영역이 없는 맵에서는 발밑과 인접 지형도 함께 공개
	bool bChanged = false;
	double NearestDistance = FMath::Square(FMath::Max(100.0,FallbackCellSize.Y));
	for (const FAreaMapRoom& Room : Rooms)
	{
		if (!Room.bAutomatic) continue;
		const FVector2D Closest(FMath::Clamp(Point.X,Room.Bounds.Min.X,Room.Bounds.Max.X),
			FMath::Clamp(Point.Y,Room.Bounds.Min.Y,Room.Bounds.Max.Y));
		const double Distance = FVector2D::DistSquared(Point,Closest);
		if (!Best && Distance < NearestDistance) { NearestDistance=Distance; }
		const FBox2D RevealBounds = Room.Bounds.ExpandBy(FVector2D(FallbackCellSize.X*0.25,FallbackCellSize.Y*0.5));
		if (RevealBounds.IsInside(Point) && !ExploredRooms.Contains(Room.Id))
		{
			MarkRoomExplored(Room); bChanged=true;
		}
	}
	if (!Best)
	{
		for (const FAreaMapRoom& Room : Rooms)
		{
			if (!Room.bAutomatic) continue;
			const FVector2D Closest(FMath::Clamp(Point.X,Room.Bounds.Min.X,Room.Bounds.Max.X),FMath::Clamp(Point.Y,Room.Bounds.Min.Y,Room.Bounds.Max.Y));
			if (FVector2D::DistSquared(Point,Closest) <= NearestDistance) { Best=&Room; break; }
		}
	}
	CurrentRoom = Best ? Best->Id : NAME_None;
	if (Best && !ExploredRooms.Contains(Best->Id)) { MarkRoomExplored(*Best); bChanged=true; }
	if (bChanged && bPersistExploration) SaveExploration();
	return bChanged;
}

void UAreaMapComponent::ComputeFogGrid(const FBox2D& Bounds, int32& OutColumns, int32& OutRows) const
{
	const FVector2D Size = Bounds.GetSize();
	const double Cell = FMath::Max(FogCellSize, 50.f);
	OutColumns = FMath::Clamp(FMath::CeilToInt(Size.X / Cell), 1, MaxFogDimension);
	OutRows = FMath::Clamp(FMath::CeilToInt(Size.Y / Cell), 1, MaxFogDimension);
}

FAreaMapFog& UAreaMapComponent::CreateFog(const FAreaMapRoom& Room)
{
	FAreaMapFog& Created = Fog.FindOrAdd(Room.Id);
	ComputeFogGrid(Room.Bounds, Created.Columns, Created.Rows);
	Created.Cells.Init(0, Created.Columns * Created.Rows);
	Created.Revealed = 0;
	return Created;
}

void UAreaMapComponent::MarkRoomExplored(const FAreaMapRoom& Room)
{
	ExploredRooms.Add(Room.Id);
	// 새로 들어간 방은 안개가 낀 채로 시작한다. 칸 기록이 없는 방은 이전 세이브의 "전체 탐사"로 취급되기 때문에 여기서 만들어 둔다.
	if (!Fog.Contains(Room.Id)) CreateFog(Room);
}

bool UAreaMapComponent::RevealFromView(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || Rooms.IsEmpty() || RevealRadius < 1.f) return false;

	// 가만히 있으면 다시 계산하지 않는다. 칸 절반 이상 움직였을 때만 갱신한다.
	const FVector2D Origin(Location.X, Location.Z);
	const double MoveThreshold = FMath::Max(FogCellSize * 0.5, 1.0);
	if (bHasRevealOrigin && FVector2D::DistSquared(Origin, LastRevealOrigin) < FMath::Square(MoveThreshold)) return false;
	bHasRevealOrigin = true;
	LastRevealOrigin = Origin;

	const double Radius = RevealRadius;
	const int32 RayCount = FMath::Clamp(RevealRayCount, 8, 360);
	const double AngleStep = 2.0 * PI / RayCount;

	// 사방으로 레이를 쏴서 각 방향에서 벽까지의 거리를 구한다(플레이어가 서 있는 평면).
	TArray<float> Distances;
	Distances.SetNumUninitialized(RayCount);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AreaMapReveal), false, GetOwner());
	for (int32 Index = 0; Index < RayCount; ++Index)
	{
		const double Angle = AngleStep * Index;
		const FVector Direction(FMath::Cos(Angle), 0.0, FMath::Sin(Angle));
		double Distance = Radius;
		for (int32 Attempt = 0; Attempt < 6; ++Attempt)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Location, Location + Direction * Radius, RevealTraceChannel, Params)) break;
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (HitComponent)
			{
				// 몬스터·물리 물체·보이지 않는 벽·시작부터 겹쳐 있는 물체는 시야를 막지 않는다(지도 지형 판정과 같은 기준).
				const ECollisionChannel ObjectType = HitComponent->GetCollisionObjectType();
				const FName Profile = HitComponent->GetCollisionProfileName();
				if (Hit.bStartPenetrating || (ObjectType != ECC_WorldStatic && ObjectType != ECC_WorldDynamic)
					|| Profile == TEXT("InvisibleWall") || Profile == TEXT("InvisibleWallDynamic"))
				{
					Params.AddIgnoredComponent(HitComponent);
					continue;
				}
			}
			Distance = Hit.Distance;
			break;
		}
		Distances[Index] = Distance;
	}

	// 반경 안의 칸 중심이 시야 안(벽까지의 거리 이내)이면 그 칸을 밝힌다.
	const FBox2D ViewBox(Origin - FVector2D(Radius, Radius), Origin + FVector2D(Radius, Radius));
	bool bChanged = false;
	for (const FAreaMapRoom& Room : Rooms)
	{
		if (!Room.Bounds.Intersect(ViewBox)) continue;

		FAreaMapFog* RoomFog = Fog.Find(Room.Id);
		// 칸 기록이 없는데 들어가 본 방은 이전 세이브의 전체 탐사 방이다. 이미 다 밝혀져 있다.
		if (!RoomFog && ExploredRooms.Contains(Room.Id)) continue;

		int32 Columns = 0, Rows = 0;
		if (RoomFog) { Columns = RoomFog->Columns; Rows = RoomFog->Rows; }
		else ComputeFogGrid(Room.Bounds, Columns, Rows);
		const FVector2D Size = Room.Bounds.GetSize();
		if (Size.X < 1.0 || Size.Y < 1.0 || Columns < 1 || Rows < 1) continue;
		const double CellW = Size.X / Columns, CellH = Size.Y / Rows;
		const double Margin = 0.5 * FMath::Max(CellW, CellH);

		const int32 C0 = FMath::Clamp(FMath::FloorToInt((ViewBox.Min.X - Room.Bounds.Min.X) / CellW), 0, Columns - 1);
		const int32 C1 = FMath::Clamp(FMath::FloorToInt((ViewBox.Max.X - Room.Bounds.Min.X) / CellW), 0, Columns - 1);
		const int32 R0 = FMath::Clamp(FMath::FloorToInt((ViewBox.Min.Y - Room.Bounds.Min.Y) / CellH), 0, Rows - 1);
		const int32 R1 = FMath::Clamp(FMath::FloorToInt((ViewBox.Max.Y - Room.Bounds.Min.Y) / CellH), 0, Rows - 1);

		// 플레이어가 서 있는 칸은 벽에 바짝 붙어 있어도 항상 밝힌다.
		const bool bOriginInside = Room.Bounds.IsInside(Origin);
		const int32 OriginColumn = FMath::Clamp(FMath::FloorToInt((Origin.X - Room.Bounds.Min.X) / CellW), 0, Columns - 1);
		const int32 OriginRow = FMath::Clamp(FMath::FloorToInt((Origin.Y - Room.Bounds.Min.Y) / CellH), 0, Rows - 1);

		for (int32 Row = R0; Row <= R1; ++Row)
		{
			for (int32 Column = C0; Column <= C1; ++Column)
			{
				if (RoomFog && RoomFog->IsRevealed(Column, Row)) continue;

				bool bSeen = bOriginInside && Column == OriginColumn && Row == OriginRow;
				if (!bSeen)
				{
					const FVector2D Delta(Room.Bounds.Min.X + (Column + 0.5) * CellW - Origin.X,
						Room.Bounds.Min.Y + (Row + 0.5) * CellH - Origin.Y);
					const double Dist = Delta.Size();
					if (Dist > Radius + Margin) continue;
					double Angle = FMath::Atan2(Delta.Y, Delta.X);
					if (Angle < 0.0) Angle += 2.0 * PI;
					const double Position = Angle / AngleStep;
					const int32 Lower = FMath::FloorToInt(Position) % RayCount;
					const double Fraction = Position - FMath::FloorToDouble(Position);
					const double WallDist = FMath::Lerp((double)Distances[Lower], (double)Distances[(Lower + 1) % RayCount], Fraction);
					const double Limit = WallDist >= Radius - 1.0 ? Radius : WallDist + Margin;
					bSeen = Dist <= Limit;
				}
				if (!bSeen) continue;

				if (!RoomFog) RoomFog = &CreateFog(Room);
				RoomFog->Cells[Row * RoomFog->Columns + Column] = 1;
				++RoomFog->Revealed;
				bChanged = true;
			}
		}
	}
	if (bChanged && bPersistExploration) SaveExploration();
	return bChanged;
}

bool UAreaMapComponent::IsPointRevealed(const FVector2D& WorldPoint) const
{
	for (const FAreaMapRoom& Room : Rooms)
	{
		if (!Room.Bounds.IsInside(WorldPoint)) continue;
		const FAreaMapFog* RoomFog = Fog.Find(Room.Id);
		if (!RoomFog)
		{
			if (ExploredRooms.Contains(Room.Id)) return true; // 이전 세이브: 방 전체가 밝혀져 있다.
			continue;
		}
		const FVector2D Size = Room.Bounds.GetSize();
		if (Size.X < 1.0 || Size.Y < 1.0 || RoomFog->Columns < 1 || RoomFog->Rows < 1) continue;
		const int32 Column = FMath::Clamp(FMath::FloorToInt((WorldPoint.X - Room.Bounds.Min.X) / (Size.X / RoomFog->Columns)), 0, RoomFog->Columns - 1);
		const int32 Row = FMath::Clamp(FMath::FloorToInt((WorldPoint.Y - Room.Bounds.Min.Y) / (Size.Y / RoomFog->Rows)), 0, RoomFog->Rows - 1);
		if (RoomFog->IsRevealed(Column, Row)) return true;
	}
	return false;
}

bool UAreaMapComponent::IsRoomShown(FName Id) const
{
	if (ExploredRooms.Contains(Id)) return true;
	const FAreaMapFog* RoomFog = Fog.Find(Id);
	return RoomFog && RoomFog->Revealed > 0;
}

void UAreaMapComponent::RevealRoom(FName Id)
{
	const FAreaMapRoom* Room = Rooms.FindByPredicate([Id](const FAreaMapRoom& Candidate) { return Candidate.Id == Id; });
	if (!Room) return;
	FAreaMapFog* RoomFog = Fog.Find(Id);
	if (!RoomFog) RoomFog = &CreateFog(*Room);
	for (uint8& Cell : RoomFog->Cells) Cell = 1;
	RoomFog->Revealed = RoomFog->Cells.Num();
}

float UAreaMapComponent::GetExplorationPercent() const
{
	double Total = 0.0, Seen = 0.0;
	for (const FAreaMapRoom& Room : Rooms)
	{
		const FAreaMapFog* RoomFog = Fog.Find(Room.Id);
		int32 Columns = 0, Rows = 0;
		ComputeFogGrid(Room.Bounds, Columns, Rows);
		const double Cells = (double)Columns * Rows;
		Total += Cells;
		if (!RoomFog) { if (ExploredRooms.Contains(Room.Id)) Seen += Cells; }
		else if (RoomFog->Cells.Num() > 0) Seen += Cells * RoomFog->Revealed / RoomFog->Cells.Num();
	}
	return Total > 0.0 ? (float)(Seen / Total * 100.0) : 0.f;
}

void UAreaMapComponent::ExportFog(TMap<FName, FProgressRoomFog>& Out) const
{
	Out.Reset();
	for (const TPair<FName, FAreaMapFog>& Pair : Fog)
	{
		FProgressRoomFog& Saved = Out.Add(Pair.Key);
		Saved.Columns = Pair.Value.Columns;
		Saved.Rows = Pair.Value.Rows;
		Saved.Bits.Init(0, (Pair.Value.Cells.Num() + 7) / 8);
		for (int32 Index = 0; Index < Pair.Value.Cells.Num(); ++Index)
			if (Pair.Value.Cells[Index]) Saved.Bits[Index >> 3] |= (uint8)(1 << (Index & 7));
	}
}

void UAreaMapComponent::ImportFog(const TMap<FName, FProgressRoomFog>* Source)
{
	Fog.Reset();
	bHasRevealOrigin = false;
	if (!Source) return;
	for (const TPair<FName, FProgressRoomFog>& Pair : *Source)
	{
		const FProgressRoomFog& Saved = Pair.Value;
		if (Saved.Columns < 1 || Saved.Rows < 1 || Saved.Columns > MaxFogDimension || Saved.Rows > MaxFogDimension) continue;
		const int32 Count = Saved.Columns * Saved.Rows;
		if (Saved.Bits.Num() < (Count + 7) / 8) continue; // 손상된 기록은 버린다.
		FAreaMapFog& Loaded = Fog.Add(Pair.Key);
		Loaded.Columns = Saved.Columns;
		Loaded.Rows = Saved.Rows;
		Loaded.Cells.Init(0, Count);
		Loaded.Revealed = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Saved.Bits[Index >> 3] & (1 << (Index & 7))) { Loaded.Cells[Index] = 1; ++Loaded.Revealed; }
		}
	}
}

void UAreaMapComponent::RestoreExploration(const TSet<FName>& RoomsToRestore, const TMap<FName, FProgressRoomFog>* CellsToRestore)
{
	ExploredRooms = RoomsToRestore;
	ImportFog(CellsToRestore);
	CurrentRoom = NAME_None;
	bLoadedExploration = true;
}

FText UAreaMapComponent::GetCurrentRoomName() const
{
	for (const FAreaMapRoom& Room : Rooms) if (Room.Id == CurrentRoom) return Room.Name;
	return NSLOCTEXT("AreaMap", "Uncharted", "미탐사 지역");
}

int32 UAreaMapComponent::GetExploredRoomCount() const
{
	return Rooms.FilterByPredicate([&](const FAreaMapRoom& Room) { return IsExplored(Room.Id); }).Num();
}

FString UAreaMapComponent::GetExplorationSlot() const
{
	const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UProgressSaveSubsystem* Saves = Instance ? Instance->GetSubsystem<UProgressSaveSubsystem>() : nullptr;
	return Saves && Saves->GetActiveSlot() > 0
		? FString::Printf(TEXT("Progress_%d"), Saves->GetActiveSlot()) : FString();
}

void UAreaMapComponent::SaveExploration()
{
	if (!bPersistExploration || !GetWorld() || !GetWorld()->GetGameInstance()) return;
	UProgressSaveSubsystem* Saves = GetWorld()->GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (UProgressSaveGame* Save = Saves->GetWorkingProgress())
	{
		FProgressMapState& State = Save->Maps.FindOrAdd(FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
		State.ExploredRooms = ExploredRooms;
		ExportFog(State.RevealedCells);
	}
	// Exploration updates memory only; checkpoint/autosave commits the active slot.
}

void UAreaMapComponent::LoadExploration()
{
	bLoadedExploration = true;
	if (!bPersistExploration || !GetWorld() || !GetWorld()->GetGameInstance()) return;
	const UProgressSaveSubsystem* Saves = GetWorld()->GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (const UProgressSaveGame* Save = Saves->GetWorkingProgress())
	{
		const FProgressMapState* Map = Save->Maps.Find(FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
		ExploredRooms = Map ? Map->ExploredRooms : TSet<FName>();
		ImportFog(Map ? &Map->RevealedCells : nullptr);
	}
}
