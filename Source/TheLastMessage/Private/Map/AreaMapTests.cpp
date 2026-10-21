#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Map/AreaMapComponent.h"
#include "Map/AreaMapRegion.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Framework/SideScrollingPlayerController.h"
#include "Character/Player/PlayerBase.h"
#include "UObject/UnrealType.h"
#include "GameFramework/WorldSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaMapTest,"TheLastMessage.Map.LayoutExplorationAndInput",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FAreaMapTest::RunTest(const FString& Parameters)
{
	UInputAction* Action=LoadObject<UInputAction>(nullptr,TEXT("/Game/Main/Inputs/Actions/IA_Map.IA_Map"));
	UInputMappingContext* Context=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Main/Inputs/IMC_SideScroller.IMC_SideScroller"));
	if (!TestNotNull(TEXT("IA_Map exists"),Action)||!TestNotNull(TEXT("Mapping context exists"),Context)) return false;
	TestTrue(TEXT("M is mapped to IA_Map"),Context->GetMappings().ContainsByPredicate([&](const FEnhancedActionKeyMapping& Mapping){return Mapping.Action==Action&&Mapping.Key==EKeys::M;}));
	TestEqual(TEXT("Controller inherits map action"),GetDefault<ASideScrollingPlayerController>()->MapAction.Get(),Action);
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player=World->SpawnActor<APlayerBase>();
	UAreaMapComponent* Map=Player->FindComponentByClass<UAreaMapComponent>();
	if (!TestNotNull(TEXT("Player owns map component"),Map)) {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
	Map->bPersistExploration=false;
	AStaticMeshActor* Floor=World->SpawnActor<AStaticMeshActor>();
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(10,4,1));Floor->SetActorLocation(FVector(0,0,-50));
	AAreaMapRegion* Room=World->SpawnActor<AAreaMapRegion>();
	Room->RegionId=TEXT("StableRoom");Room->RegionName=FText::FromString(TEXT("Test room"));
	Room->RegionBounds->SetBoxExtent(FVector(500,500,400));Room->SetActorLocation(FVector(0,0,400));
	Room->LayoutActors.Add(Floor);Room->CaptureLayoutBounds();
	Map->RefreshLayout();
	TestFalse(TEXT("Room starts unexplored"),Map->IsExplored(Room->RegionId));
	TestTrue(TEXT("Entering reveals whole room"),Map->ExploreAt(FVector(0,0,100)));
	TestTrue(TEXT("Exploration state is recorded"),Map->IsExplored(Room->RegionId));
	TestFalse(TEXT("Repeated visits do not rediscover"),Map->ExploreAt(FVector(0,0,100)));
	const FBox2D Before=Room->GetMapBounds();
	Floor->SetActorLocation(FVector(1000,0,-50));
	Map->RefreshLayout();
	TestTrue(TEXT("Moving terrain updates room bounds"),FMath::IsNearlyEqual(Room->GetMapBounds().Min.X,Before.Min.X+1000,0.1));
	TestTrue(TEXT("Moving terrain preserves exploration ID"),Map->IsExplored(Room->RegionId));
	TestTrue(TEXT("Actual map geometry follows moved floor"),Map->GetSurfaces().ContainsByPredicate([](const FBox2D& Bounds){return Bounds.Min.X>=499&&Bounds.Max.X>=1499;}));
	UAreaMapSaveGame* Save=NewObject<UAreaMapSaveGame>();Save->ExploredRooms.Add(Room->RegionId);
	TArray<uint8> SaveBytes;
	UGameplayStatics::SaveGameToMemory(Save,SaveBytes);
	UAreaMapSaveGame* Loaded=Cast<UAreaMapSaveGame>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
	TestTrue(TEXT("Save serialization retains stable room identifiers"),Loaded&&Loaded->ExploredRooms.Contains(Room->RegionId));
	World->EndPlay(EEndPlayReason::Quit);World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaMapOffsetPlaneTest,"TheLastMessage.Map.OffsetPlaneWithoutRegions",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FAreaMapOffsetPlaneTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player=World->SpawnActor<APlayerBase>();Player->SetActorLocation(FVector(100,1000,100));
	UAreaMapComponent* Map=Player->FindComponentByClass<UAreaMapComponent>();Map->bPersistExploration=false;
	auto MakeFloor=[&](double Y)
	{
		AStaticMeshActor* Actor=World->SpawnActor<AStaticMeshActor>();
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Actor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Actor->SetActorScale3D(FVector(10,4,1));Actor->SetActorLocation(FVector(0,Y,-50));
	};
	MakeFloor(1000);MakeFloor(0);
	Map->RefreshLayout();
	TestTrue(TEXT("Offset test map has valid map bounds without room markers"),Map->GetMapBounds().bIsValid);
	TestEqual(TEXT("Only player's actual plane is collected"),Map->GetSurfaces().Num(),1);
	TestTrue(TEXT("Adjacent automatic cells are not accidentally skipped"),Map->GetRooms().Num()>=4);
	TestTrue(TEXT("Unmarked test map can be explored"),Map->ExploreAt(Player->GetActorLocation()));
	TestFalse(TEXT("Current location is associated with an automatic area"),Map->GetCurrentRoomId().IsNone());
	TestTrue(TEXT("Floor below player's feet is revealed"),Map->GetRooms().ContainsByPredicate([&](const FAreaMapRoom& Room)
		{return Room.Bounds.Min.Y<0&&Map->IsExplored(Room.Id);}));
	World->EndPlay(EEndPlayReason::Quit);World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
	return !HasAnyErrors();
}
#endif
