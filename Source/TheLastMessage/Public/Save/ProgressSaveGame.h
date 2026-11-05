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

USTRUCT(BlueprintType)
struct FProgressMapState
{
	GENERATED_BODY()
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
	UPROPERTY(SaveGame) FTransform RespawnTransform;
	UPROPERTY(SaveGame) FVector RespawnGravity = FVector(0, 0, -1);
	UPROPERTY(SaveGame) FProgressPlayerState Player;
	/** Placed pickup IDs captured with the checkpoint, independently of owned core types/levels. */
	UPROPERTY(SaveGame) TMap<FGuid, bool> CorePickupStates;
	UPROPERTY(SaveGame) TMap<FName, FProgressMapState> Maps;
};
