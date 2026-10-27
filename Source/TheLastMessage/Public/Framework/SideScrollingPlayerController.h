// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInput/Public/InputAction.h"
#include "UI/PauseMenuWidget.h"
#include "SideScrollingPlayerController.generated.h"

class ASideScrollingCharacter;
class UInputMappingContext;
class UAreaMapWidget;

/**
 *  A simple Side Scrolling Player Controller
 *  Manages input mappings
 *  Respawns the player pawn at the player start if it is destroyed
 */
UCLASS(abstract, Config="Game")
class ASideScrollingPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ASideScrollingPlayerController();
	UFUNCTION(BlueprintCallable, Category="Area Map") void ToggleAreaMap();
	UFUNCTION(BlueprintCallable, Category="Pause Menu") void TogglePauseMenu();
	UFUNCTION(BlueprintPure, Category="Pause Menu") UPauseMenuWidget* GetPauseMenu() const { return PauseMenuWidget; }
	UPROPERTY(EditDefaultsOnly, Config, Category="Pause Menu") TSoftClassPtr<UPauseMenuWidget> PauseMenuClass;
	UPROPERTY(EditDefaultsOnly, Category="Pause Menu") TSoftObjectPtr<UWorld> MainMenuMap;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Pause Menu") TObjectPtr<UInputAction> PauseMenuAction;
	/** 구역 지도를 열고 닫는 입력 액션 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Area Map", meta=(ToolTip="구역 지도를 열고 닫는 입력 액션"))
	TObjectPtr<UInputAction> MapAction;
	/** 구역 지도 화면 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Area Map", meta=(ToolTip="구역 지도 화면 클래스"))
	TSubclassOf<UAreaMapWidget> MapWidgetClass;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(Transient) TObjectPtr<UAreaMapWidget> AreaMapWidget;

	/** Input mapping context for this player */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Respawn")
	TSubclassOf<ASideScrollingCharacter> CharacterClass;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
	void ClosePauseMenu();
	void ExecutePauseAction(EPauseMenuAction Action);
	UFUNCTION() void HandlePauseAction(EPauseMenuAction Action);
	UFUNCTION() void PauseSaveFinished(bool bSuccess);
	UFUNCTION() void PauseMenuRemoved();
	UPROPERTY(Transient) TObjectPtr<UPauseMenuWidget> PauseMenuWidget;
	bool bPausedByMenu = false;
	bool bPausePreviousCursor = false;
	bool bWaitingForPauseSave = false;
	EPauseMenuAction PendingPauseAction = EPauseMenuAction::Resume;

};
