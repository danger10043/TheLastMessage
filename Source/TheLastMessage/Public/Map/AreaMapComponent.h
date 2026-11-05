#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "Engine/EngineTypes.h"
#include "Save/ProgressSaveGame.h"
#include "AreaMapComponent.generated.h"

class UTexture2D;

USTRUCT()
struct FAreaMapRoom
{
	GENERATED_BODY()
	FName Id;
	FText Name;
	FBox2D Bounds = FBox2D(ForceInit);
	TWeakObjectPtr<UTexture2D> Silhouette;
	bool bAutomatic = false;
};

/** 방 하나의 안개 상태. 방을 Columns x Rows 칸으로 나눠 시야가 닿은 칸을 1로 기록한다(칸 번호 = Row * Columns + Column, Row 0이 방의 맨 아래). */
struct FAreaMapFog
{
	int32 Columns = 0;
	int32 Rows = 0;
	TArray<uint8> Cells;
	int32 Revealed = 0;

	bool IsRevealed(int32 Column, int32 Row) const
	{
		return Column >= 0 && Column < Columns && Row >= 0 && Row < Rows && Cells.IsValidIndex(Row * Columns + Column) && Cells[Row * Columns + Column] != 0;
	}
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
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** 탐사 기록을 저장 파일에 유지 */
	UPROPERTY(EditAnywhere, Category="Area Map", meta=(ToolTip="탐사 기록을 저장 파일에 유지"))
	bool bPersistExploration = true;
	/** 방 영역이 없는 장소에서 사용하는 자동 탐사 칸 크기 */
	UPROPERTY(EditAnywhere, Category="Area Map", meta=(ClampMin="100", ToolTip="방 영역이 없는 장소에서 사용하는 자동 탐사 칸 크기"))
	FVector2D FallbackCellSize = FVector2D(600,400);

	/** 지도 안개 칸의 크기(cm). 방 하나를 이 크기로 나눠 시야가 닿은 칸만 지도에 밝힌다. 작을수록 정밀하지만 칸이 늘어난다. */
	UPROPERTY(EditAnywhere, Category="Area Map|Fog", meta=(ClampMin="50", ToolTip="지도 안개 칸의 크기(cm)"))
	float FogCellSize = 150.f;

	/** 플레이어 시야가 닿는 최대 반경(cm). 이 안에서 벽에 막히지 않은 칸만 밝혀진다. */
	UPROPERTY(EditAnywhere, Category="Area Map|Fog", meta=(ClampMin="100", ToolTip="시야 반경(cm)"))
	float RevealRadius = 1000.f;

	/** 시야 계산에 쓰는 레이 개수. 많을수록 정확하고 조금 더 무겁다. */
	UPROPERTY(EditAnywhere, Category="Area Map|Fog", meta=(ClampMin="8", ClampMax="360", ToolTip="시야 계산용 레이 개수"))
	int32 RevealRayCount = 48;

	/** 시야 갱신 간격(초). 플레이어가 움직였을 때만 실제로 계산한다. */
	UPROPERTY(EditAnywhere, Category="Area Map|Fog", meta=(ClampMin="0.05", ToolTip="시야 갱신 간격(초)"))
	float RevealInterval = 0.2f;

	/** 시야를 막는 벽을 찾을 때 쓰는 트레이스 채널. 지도 지형 판정(플레이어를 막는 메시)과 같은 Pawn을 기본으로 쓴다. */
	UPROPERTY(EditAnywhere, Category="Area Map|Fog")
	TEnumAsByte<ECollisionChannel> RevealTraceChannel = ECC_Pawn;

	void RefreshLayout();
	bool ExploreAt(const FVector& Location);

	/** 플레이어 위치에서 벽에 막히지 않고 보이는 칸을 밝힌다. 새로 밝혀진 칸이 있으면 true. */
	bool RevealFromView(const FVector& Location);

	/** 지도 좌표(X, Z)의 칸이 밝혀졌는지 */
	bool IsPointRevealed(const FVector2D& WorldPoint) const;

	/** 방이 지도에 그려져야 하는지(들어가 본 방이거나 일부 칸이 밝혀진 방) */
	bool IsRoomShown(FName Id) const;

	/** 방의 안개 상태. 없으면 nullptr. 들어가 본 방인데 nullptr이면 방 전체가 밝혀진 것으로 본다(이전 세이브). */
	const FAreaMapFog* FindFog(FName Id) const { return Fog.Find(Id); }

	/** 방 전체를 밝힌다(미리보기·테스트용) */
	void RevealRoom(FName Id);

	/** 전체 칸 중 밝혀진 비율(0~100) */
	float GetExplorationPercent() const;

	/** 현재 안개 상태를 저장용 구조로 내보낸다. */
	void ExportFog(TMap<FName, FProgressRoomFog>& Out) const;
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
	/** CellsToRestore가 nullptr이면 칸 기록이 없는 것으로 보고, 들어가 본 방은 전체가 밝혀진 것으로 처리한다. */
	void RestoreExploration(const TSet<FName>& RoomsToRestore, const TMap<FName, FProgressRoomFog>* CellsToRestore = nullptr);
private:
	static constexpr int32 MaxFogDimension = 64;

	/** 방 영역을 FogCellSize로 나눈 칸 수 */
	void ComputeFogGrid(const FBox2D& Bounds, int32& OutColumns, int32& OutRows) const;
	FAreaMapFog& CreateFog(const FAreaMapRoom& Room);
	void MarkRoomExplored(const FAreaMapRoom& Room);
	void ImportFog(const TMap<FName, FProgressRoomFog>* Source);

	TMap<FName, FAreaMapFog> Fog;
	FVector2D LastRevealOrigin = FVector2D::ZeroVector;
	bool bHasRevealOrigin = false;

	TArray<FAreaMapRoom> Rooms;
	TArray<FBox2D> Surfaces;
	TArray<FVector2D> Checkpoints;
	TSet<FName> ExploredRooms;
	FBox2D MapBounds = FBox2D(ForceInit);
	FName CurrentRoom;
	float RefreshElapsed = 2;
	bool bLoadedExploration = false;
};
