#include "Map/AreaMapComponent.h"
#include "Map/AreaMapRegion.h"
#include "Components/StaticMeshComponent.h"
#include "Character/Player/PlayerBase.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Checkpoint/PlayerCheckpoint.h"

UAreaMapComponent::UAreaMapComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
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
	if (!Player->IsDead()) ExploreAt(Player->GetActorLocation());
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
			ExploredRooms.Add(Room.Id); bChanged=true;
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
	if (Best && !ExploredRooms.Contains(Best->Id)) { ExploredRooms.Add(Best->Id); bChanged=true; }
	if (bChanged && bPersistExploration) SaveExploration();
	return bChanged;
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
	return TEXT("AreaMap_") + UGameplayStatics::GetCurrentLevelName(this, true);
}

void UAreaMapComponent::SaveExploration()
{
	if (!bPersistExploration) return;
	UAreaMapSaveGame* Save = Cast<UAreaMapSaveGame>(UGameplayStatics::CreateSaveGameObject(UAreaMapSaveGame::StaticClass()));
	Save->ExploredRooms = ExploredRooms;
	UGameplayStatics::SaveGameToSlot(Save, GetExplorationSlot(), 0);
}

void UAreaMapComponent::LoadExploration()
{
	bLoadedExploration = true;
	if (!bPersistExploration) return;
	if (const UAreaMapSaveGame* Save = Cast<UAreaMapSaveGame>(UGameplayStatics::LoadGameFromSlot(GetExplorationSlot(),0)))
		ExploredRooms = Save->ExploredRooms;
}
