#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "StartMenuHUD.generated.h"

class UStartMenuWidget;

UCLASS(Blueprintable)
class THELASTMESSAGE_API AStartMenuHUD : public AHUD
{
	GENERATED_BODY()
public:
	AStartMenuHUD();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category="Menu") TSubclassOf<UStartMenuWidget> MenuClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Menu") TObjectPtr<UStartMenuWidget> Menu;
	UPROPERTY(EditDefaultsOnly, Category="Menu") TSoftObjectPtr<UWorld> NewGameMap;
private:
	void RefreshSlots();
	void TravelToProgress();
	bool bStartingGame = false;
	bool SlotExists[3] = {false, false, false};
	UFUNCTION() void SelectSlot(int32 SlotId, bool bNewGame);
	UFUNCTION() void InitialSaveFinished(bool bSuccess);
	UFUNCTION() void ExitGame();
};

/** Dedicated menu world: no gameplay pawn, combat HUD or input mappings. */
UCLASS()
class THELASTMESSAGE_API AStartMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AStartMenuGameMode();
};
