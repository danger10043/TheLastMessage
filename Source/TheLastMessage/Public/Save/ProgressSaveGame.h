#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProgressSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FProgressActorState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) FName State;
	UPROPERTY(SaveGame) bool bEnabled = true;
	UPROPERTY(SaveGame) bool bActive = false;
	UPROPERTY(SaveGame) bool bCompleted = false;
	UPROPERTY(SaveGame) FGuid RelatedActorId;
	UPROPERTY(SaveGame) FTransform ComponentTransform = FTransform::Identity;
	UPROPERTY(SaveGame, BlueprintReadWrite) FTransform Transform;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<uint8> Payload;
};

/** 방 하나의 지도 안개 기록. 방을 Columns x Rows 칸으로 나눠 시야가 닿은 칸을 비트로 저장한다(칸 번호 = Row * Columns + Column). */
USTRUCT(BlueprintType)
struct FProgressRoomFog
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 Columns = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) int32 Rows = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<uint8> Bits;
};

/** 방 하나의 지도 지형. 방이 내려가 있어도 지도에 그릴 수 있게 가 본 방의 지형을 저장한다. 네 값씩(MinX, MinZ, MaxX, MaxZ) 묶어 이어 붙인다. */
USTRUCT(BlueprintType)
struct FProgressRoomSurfaces
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<float> Boxes;
};

/** 방 하나의 지도 리전(범위와 이름). 리전 액터가 방 레벨 안에 있어서, 방이 내려가면 지도에서 방이 사라지지 않게 가 본 방의 리전을 저장한다. 범위는 네 값씩(MinX, MinZ, MaxX, MaxZ) 이어 붙인다. */
USTRUCT(BlueprintType)
struct FProgressRoomRegions
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<float> Boxes;
	UPROPERTY(SaveGame, BlueprintReadWrite) FText Name;
	UPROPERTY(SaveGame, BlueprintReadWrite) FSoftObjectPath Silhouette;
};

/** 지도에 그릴 체크포인트 정보. 체크포인트가 방 레벨 안에 있어서, 방이 내려가도 지도에 아이콘이 남게 가 본 체크포인트를 저장한다. */
USTRUCT(BlueprintType)
struct FProgressCheckpointMarker
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
	UPROPERTY(SaveGame, BlueprintReadWrite) FVector RespawnLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame, BlueprintReadWrite) FRotator RespawnRotation = FRotator::ZeroRotator;
	UPROPERTY(SaveGame, BlueprintReadWrite) bool bAllowFastTravel = false;
};

USTRUCT(BlueprintType)
struct FProgressMapState
{
	GENERATED_BODY()
	/** 체크포인트 고유 ID -> 지도 아이콘 정보(가 본 체크포인트만). 방이 내려가 있어도 지도에 아이콘을 그리고 빠른 이동에 쓴다. */
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FGuid, FProgressCheckpointMarker> CheckpointMarkers;
	/** 방 이름 -> 그 방의 지도 리전. 방 레벨이 내려가 있어도 지도에 방을 그리는 데 쓴다. */
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FName, FProgressRoomRegions> RoomRegions;
	/** 방 이름 -> 그 방의 지도 지형. 방 레벨이 내려가 있을 때 지도에 쓴다. 없는 방은 로드된 동안에 새로 읽는다. */
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FName, FProgressRoomSurfaces> RoomSurfaces;
	/** 들어가 본 방. 지도에 방 이름이 표시된다. 칸 기록(RevealedCells)이 없는 방은 방 전체가 밝혀진 것으로 본다(이전 세이브 호환). */
	UPROPERTY(SaveGame, BlueprintReadWrite) TSet<FName> ExploredRooms;
	/** 방 이름 -> 그 방에서 시야가 닿은 칸. */
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FName, FProgressRoomFog> RevealedCells;
	UPROPERTY(SaveGame, BlueprintReadWrite) TSet<FGuid> VisitedCheckpoints;
	UPROPERTY(SaveGame, BlueprintReadWrite) TSet<FName> PlayedCinematics;
	/** Authored stable IDs; never use runtime actor names as persistence IDs. */
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FGuid, FProgressActorState> Actors;
};

USTRUCT(BlueprintType)
struct FProgressPlayerState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<FSoftObjectPath> OwnedWeapons;
	UPROPERTY(SaveGame, BlueprintReadWrite) FSoftObjectPath PrimaryWeapon;
	UPROPERTY(SaveGame, BlueprintReadWrite) FSoftObjectPath SecondaryWeapon;
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FSoftObjectPath, int32> CoreLevels;
	UPROPERTY(SaveGame, BlueprintReadWrite) TMap<FName, float> PermanentUpgrades;
	/** Captured values, not permanent bonuses; restore must not reapply these as core modifiers. */
	UPROPERTY(SaveGame, BlueprintReadWrite) float Health = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) float MaxHealth = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) float Energy = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite) float MaxEnergy = 0;
};

/** One complete checkpoint snapshot per playthrough slot, including map exploration. */
UCLASS()
class THELASTMESSAGE_API UProgressSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	static constexpr int32 CurrentVersion = 1;
	UPROPERTY(SaveGame) int32 Version = CurrentVersion;
	UPROPERTY(SaveGame) int32 SlotId = 0;
	UPROPERTY(SaveGame) FGuid PlaythroughId;
	UPROPERTY(SaveGame) FDateTime SavedAt;
	UPROPERTY(SaveGame) FString MapPackage;
	UPROPERTY(SaveGame) FGuid CheckpointId;
	/** 체크포인트를 저장할 때 플레이어가 있던 리전 ID. 부활 전에 그 방을 먼저 올리는 데 쓴다(방 스트리밍이 꺼져 있으면 비어 있음). */
	UPROPERTY(SaveGame) FName CheckpointRegionId;
	UPROPERTY(SaveGame) FTransform RespawnTransform;
	UPROPERTY(SaveGame) FVector RespawnGravity = FVector(0, 0, -1);
	UPROPERTY(SaveGame) FProgressPlayerState Player;
	/** Placed pickup IDs captured with the checkpoint, independently of owned core types/levels. */
	UPROPERTY(SaveGame) TMap<FGuid, bool> CorePickupStates;
	/** 켜져 있는 월드 플래그(게임플레이 태그 이름). 맵과 상관없이 전체에서 공유한다. 체크포인트 시점의 값. */
	UPROPERTY(SaveGame) TSet<FName> WorldFlags;
	UPROPERTY(SaveGame) TMap<FName, FProgressMapState> Maps;
};
