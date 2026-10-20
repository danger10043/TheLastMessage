// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SideScrollingGameMode.generated.h"

/**
 *  Simple Side Scrolling Game Mode
 *  Counts pickups collected by the player
 */
UCLASS(abstract)
class ASideScrollingGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** Number of pickups collected by the player */
	UPROPERTY(BlueprintReadOnly, Category="Pickups")
	int32 PickupsCollected = 0;

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Assigns a PlayerStart to a specific player */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:

	/** Determines how many local players should be spawned on game start */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Used to assign players to different PlayerStarts in the level */
	int32 CurrentPlayerStartAssignment = 0;

public:

	/** Receives an interaction event from another actor */
	virtual void ProcessPickup();
};
