#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProgressSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FProgressActorState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) FName State;
	UPROPERTY(SaveGame, BlueprintReadWrite) FTransform Transform;
	UPROPERTY(SaveGame, BlueprintReadWrite) TArray<uint8> Payload;
};

USTRUCT(BlueprintType)
struct FProgressMapState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadWrite) TSet<FName> ExploredRooms;
	UPROPERTY(SaveGame, BlueprintReadWrite) TSet<FGuid> VisitedCheckpoints;
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
	UPROPERTY(SaveGame) FProgressPlayerState Player;
	UPROPERTY(SaveGame) TMap<FName, FProgressMapState> Maps;
};
