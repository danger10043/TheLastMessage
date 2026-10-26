// Copyright Epic Games, Inc. All Rights Reserved.


#include "Framework/SideScrollingPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Character/Player/SideScrollingCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "../../TheLastMessage.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputComponent.h"
#include "Map/AreaMapComponent.h"
#include "UI/AreaMapWidget.h"
#include "Character/Player/PlayerBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"

ASideScrollingPlayerController::ASideScrollingPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputAction> Action(TEXT("/Game/Main/Inputs/Actions/IA_Map.IA_Map"));
	MapAction = Action.Object;
	MapWidgetClass = UAreaMapWidget::StaticClass();
	PauseMenuClass = TSoftClassPtr<UPauseMenuWidget>(FSoftObjectPath(TEXT("/Game/Main/UI/Menu/WBP_PauseMenu.WBP_PauseMenu_C")));
	MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Main/Maps/Lv_MainMenu.Lv_MainMenu")));
}

void ASideScrollingPlayerController::ToggleAreaMap()
{
	if (AreaMapWidget && AreaMapWidget->IsInViewport()) { AreaMapWidget->CloseMap(); return; }
	APlayerBase* MapPlayer = Cast<APlayerBase>(GetPawn());
	if (!IsLocalController() || !MapPlayer || MapPlayer->IsDead() || !MapWidgetClass || UGameplayStatics::IsGamePaused(this)) return;
	UAreaMapComponent* Map = MapPlayer->FindComponentByClass<UAreaMapComponent>();
	if (!Map) return;
	AreaMapWidget = CreateWidget<UAreaMapWidget>(this,MapWidgetClass);
	if (AreaMapWidget) AreaMapWidget->OpenMap(Map,MapAction);
}

void ASideScrollingPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bWaitingForPauseSave = false;
	ClosePauseMenu();
	if (GetGameInstance()) GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->OnSaveFinished.RemoveDynamic(this, &ASideScrollingPlayerController::PauseSaveFinished);
	if (AreaMapWidget) AreaMapWidget->CloseMap();
	AreaMapWidget=nullptr;
	Super::EndPlay(EndPlayReason);
}

void ASideScrollingPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && !ShouldUseTouchControls())
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void ASideScrollingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASideScrollingPlayerController::TogglePauseMenu).bExecuteWhenPaused = true;
	if (UEnhancedInputComponent* Inputs = Cast<UEnhancedInputComponent>(InputComponent); Inputs && MapAction)
		Inputs->BindAction(MapAction,ETriggerEvent::Started,this,&ASideScrollingPlayerController::ToggleAreaMap);

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogTemp, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ASideScrollingPlayerController::TogglePauseMenu()
{
	if (PauseMenuWidget && PauseMenuWidget->IsInViewport()) { HandlePauseAction(EPauseMenuAction::Resume); return; }
	if (!IsLocalController() || !GetPawn() || !GetGameInstance() || GetWorld()->GetNetMode() != NM_Standalone
		|| UGameplayStatics::IsGamePaused(this)) return;
	if (APlayerBase* InGamePlayer = Cast<APlayerBase>(GetPawn())) InGamePlayer->CancelWeaponInputsForUI();
	UClass* MenuClass = PauseMenuClass.LoadSynchronous();
	PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this, MenuClass ? MenuClass : UPauseMenuWidget::StaticClass());
	if (!PauseMenuWidget || !PauseMenuWidget->AddToPlayerScreen(200)) { PauseMenuWidget = nullptr; return; }
	bPausedByMenu = SetPause(true);
	if (!bPausedByMenu) { PauseMenuWidget->RemoveFromParent(); PauseMenuWidget = nullptr; return; }
	bPausePreviousCursor = bShowMouseCursor;
	UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	PauseMenuWidget->OnActionRequested.AddUniqueDynamic(this, &ASideScrollingPlayerController::HandlePauseAction);
	PauseMenuWidget->OnMenuRemoved.AddUniqueDynamic(this, &ASideScrollingPlayerController::PauseMenuRemoved);
	PauseMenuWidget->SetCheckpointAvailable(Saves->GetSlotInfo(Saves->GetActiveSlot()).bHasCheckpoint || Saves->IsSaving());
	Saves->OnSaveFinished.AddUniqueDynamic(this, &ASideScrollingPlayerController::PauseSaveFinished);
	FInputModeUIOnly Input; Input.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Input); SetShowMouseCursor(true); FlushPressedKeys(); PauseMenuWidget->SetKeyboardFocus();
}

void ASideScrollingPlayerController::ClosePauseMenu()
{
	if (!PauseMenuWidget || bWaitingForPauseSave) return;
	UPauseMenuWidget* ClosingWidget = PauseMenuWidget;
	PauseMenuWidget = nullptr;
	ClosingWidget->OnMenuRemoved.RemoveDynamic(this, &ASideScrollingPlayerController::PauseMenuRemoved);
	ClosingWidget->OnActionRequested.RemoveDynamic(this, &ASideScrollingPlayerController::HandlePauseAction);
	ClosingWidget->RemoveFromParent();
	if (GetWorld() && !GetWorld()->bIsTearingDown)
	{
		FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false);
		Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Input); SetShowMouseCursor(bPausePreviousCursor); FlushPressedKeys();
		if (bPausedByMenu) SetPause(false);
	}
	bPausedByMenu = false;
}

void ASideScrollingPlayerController::PauseMenuRemoved()
{
	bWaitingForPauseSave = false;
	ClosePauseMenu();
}

void ASideScrollingPlayerController::HandlePauseAction(EPauseMenuAction Action)
{
	if (!PauseMenuWidget || bWaitingForPauseSave) return;
	UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (Action != EPauseMenuAction::Resume && Saves->IsSaving())
	{
		bWaitingForPauseSave = true; PendingPauseAction = Action; PauseMenuWidget->SetBusy(true); return;
	}
	ExecutePauseAction(Action);
}

void ASideScrollingPlayerController::PauseSaveFinished(bool bSuccess)
{
	if (!PauseMenuWidget) return;
	UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (Saves->IsSaving()) return; // A second checkpoint snapshot may still be queued.
	PauseMenuWidget->SetCheckpointAvailable(Saves->GetSlotInfo(Saves->GetActiveSlot()).bHasCheckpoint);
	if (!bWaitingForPauseSave) return;
	bWaitingForPauseSave = false;
	if (!bSuccess) { PauseMenuWidget->ShowError(NSLOCTEXT("Pause", "SaveFailed", "저장에 실패했습니다. 이동을 취소했습니다.")); return; }
	PauseMenuWidget->SetBusy(false); ExecutePauseAction(PendingPauseAction);
}

void ASideScrollingPlayerController::ExecutePauseAction(EPauseMenuAction Action)
{
	if (Action == EPauseMenuAction::Resume) { ClosePauseMenu(); return; }
	UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (Action == EPauseMenuAction::ReloadCheckpoint)
	{
		if (!Saves->ReloadActiveProgress()) { PauseMenuWidget->ShowError(NSLOCTEXT("Pause", "LoadFailed", "체크포인트를 불러오지 못했습니다.")); return; }
		const FString Map = Saves->GetWorkingProgress()->MapPackage;
		ClosePauseMenu(); UGameplayStatics::OpenLevel(this, FName(*Map)); return;
	}
	if (Action == EPauseMenuAction::MainMenu)
	{
		const FString Map = MainMenuMap.ToSoftObjectPath().GetLongPackageName();
		if (!FPackageName::DoesPackageExist(Map)) { PauseMenuWidget->ShowError(NSLOCTEXT("Pause", "MissingMenu", "시작 화면을 열지 못했습니다.")); return; }
		if (!Saves->EndSession()) return;
		ClosePauseMenu(); UGameplayStatics::OpenLevel(this, FName(*Map)); return;
	}
	if (Action == EPauseMenuAction::QuitGame)
	{
		ClosePauseMenu(); UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

void ASideScrollingPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	InPawn->OnDestroyed.AddDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
}

void ASideScrollingPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// spawn a character at the player start
		const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

		if (ASideScrollingCharacter* RespawnedCharacter = GetWorld()->SpawnActor<ASideScrollingCharacter>(CharacterClass, SpawnTransform))
		{
			// possess the character
			Possess(RespawnedCharacter);
		}
	}
}

bool ASideScrollingPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
