#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "AreaMapComponent.generated.h"

USTRUCT()
struct FAreaMapRoom
{
	GENERATED_BODY()
	FName Id;
	FText Name;
	FBox2D Bounds = FBox2D(ForceInit);
	bool bAutomatic = false;
};

UCLASS()
class THELASTMESSAGE_API UAreaMapSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) TSet<FName> ExploredRooms;
};

/** 실제 지형에서 지도 형태를 만들고 방별 탐사 기록 관리 */
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class THELASTMESSAGE_API UAreaMapComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAreaMapComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** 탐사 기록을 저장 파일에 유지 */
	UPROPERTY(EditAnywhere, Category="Area Map", meta=(ToolTip="탐사 기록을 저장 파일에 유지"))
	bool bPersistExploration = true;
	/** 방 영역이 없는 장소에서 사용하는 자동 탐사 칸 크기 */
	UPROPERTY(EditAnywhere, Category="Area Map", meta=(ClampMin="100", ToolTip="방 영역이 없는 장소에서 사용하는 자동 탐사 칸 크기"))
	FVector2D FallbackCellSize = FVector2D(600,400);
	void RefreshLayout();
	bool ExploreAt(const FVector& Location);
	bool IsExplored(FName Id) const { return ExploredRooms.Contains(Id); }
	const TArray<FAreaMapRoom>& GetRooms() const { return Rooms; }
	const TArray<FBox2D>& GetSurfaces() const { return Surfaces; }
	const TArray<FVector2D>& GetCheckpoints() const { return Checkpoints; }
	FBox2D GetMapBounds() const { return MapBounds; }
	FName GetCurrentRoomId() const { return CurrentRoom; }
	FText GetCurrentRoomName() const;
	int32 GetExploredRoomCount() const;
	FString GetExplorationSlot() const;
	void SaveExploration();
	void LoadExploration();
	void RestoreExploration(const TSet<FName>& RoomsToRestore) { ExploredRooms = RoomsToRestore; CurrentRoom = NAME_None; bLoadedExploration = true; }
private:
	TArray<FAreaMapRoom> Rooms;
	TArray<FBox2D> Surfaces;
	TArray<FVector2D> Checkpoints;
	TSet<FName> ExploredRooms;
	FBox2D MapBounds = FBox2D(ForceInit);
	FName CurrentRoom;
	float RefreshElapsed = 2;
	bool bLoadedExploration = false;
};
