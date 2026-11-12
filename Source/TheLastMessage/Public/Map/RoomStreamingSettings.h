#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RoomStreamingSettings.generated.h"

class UWorld;

/** 방 하나: 로드할 레벨 인스턴스와, 그 방에 속한 리전 ID들. */
USTRUCT()
struct FRoomStreamEntry
{
	GENERATED_BODY()

	/** 방 번호. 예: 08 */
	UPROPERTY(EditAnywhere, Category = "Room")
	FName RoomId;

	/** 맵에 배치된 레벨 인스턴스가 가리키는 방 레벨. 예: /Game/Main/Maps/LI_08 */
	UPROPERTY(EditAnywhere, Category = "Room")
	TSoftObjectPtr<UWorld> Level;

	/** 이 방 안에 있을 때 현재 방으로 인정하는 AreaMapRegion의 RegionId들. 예: R_08 */
	UPROPERTY(EditAnywhere, Category = "Room")
	TArray<FName> RegionIds;
};

/** 복도 하나: 이어진 방들. 복도에 있을 때는 양쪽 끝(또는 갈래) 방을 모두 올린다. */
USTRUCT()
struct FRoomCorridorEntry
{
	GENERATED_BODY()

	/** 복도 이름이자 리전 ID. 예: RH_0809 */
	UPROPERTY(EditAnywhere, Category = "Corridor")
	FName CorridorId;

	/** 이 복도로 이어진 방 번호들. 예: 08, 09 */
	UPROPERTY(EditAnywhere, Category = "Corridor")
	TArray<FName> RoomIds;
};

/**
 * 플레이어 주변 방만 올리고 나머지는 내리는 방 단위 스트리밍 설정.
 * 꺼져 있으면(기본) 아무것도 바꾸지 않는다. 설정은 DefaultGame.ini에 저장된다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Room Streaming"))
class THELASTMESSAGE_API URoomStreamingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URoomStreamingSettings() { CategoryName = TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category = "Room Streaming")
	bool bEnabled = false;

	/** 방이 필요 없어진 뒤 이만큼(초) 지나야 내린다. 경계를 오갈 때 방이 깜빡이며 올라갔다 내려가는 것을 막는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Room Streaming", meta = (ClampMin = "0"))
	float UnloadDelaySeconds = 6.f;

	/**
	 * 체크포인트 부활 때 필요한 방이 다 올라온 뒤 충돌과 바닥이 자리 잡을 때까지 더 기다리는 시간(초).
	 * 부활 위치에서 바닥이 확인되기 전에는 이 시간이 지나도 부활하지 않는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Room Streaming", meta = (ClampMin = "0"))
	float RespawnSettleSeconds = 0.75f;

	/** 플레이어가 어느 리전에 있는지 확인하는 간격(초). */
	UPROPERTY(Config, EditAnywhere, Category = "Room Streaming", meta = (ClampMin = "0.05"))
	float UpdateIntervalSeconds = 0.25f;

	/** 목록에 없는 방은 관리하지 않고 항상 로드된 채로 둔다. */
	UPROPERTY(Config, EditAnywhere, Category = "Room Streaming")
	TArray<FRoomStreamEntry> Rooms;

	UPROPERTY(Config, EditAnywhere, Category = "Room Streaming")
	TArray<FRoomCorridorEntry> Corridors;
};
