// Copyright Epic Games, Inc. All Rights Reserved.


#include "Framework/SideScrollingPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UI/GameKeySettingsSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Character/Player/SideScrollingCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
// +수정----
#include "TheLastMessage.h"
// +--------
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputComponent.h"
#include "Map/AreaMapComponent.h"
#include "UI/AreaMapWidget.h"
#include "Character/Player/PlayerBase.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "UI/PlayerHUD.h"
#include "Character/Boss/BossMonster.h"
#include "Cinematics/OnceCinematicController.h"
#include "EngineUtils.h"
#include "UI/FactoryCursor.h"
#include "DataAsset/WeaponDataAsset.h"
// +추가----
#include "Components/SceneComponent.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
// +--------

namespace
{
	class FBossSequenceInputProcessor final : public IInputProcessor
	{
	public:
		FBossSequenceInputProcessor(
			ASideScrollingPlayerController* InController, TFunction<void()> InSkip)
			: Controller(InController), Skip(MoveTemp(InSkip)) {}

		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			if (!ShouldBlock()) return false;
			if (Event.GetKey() == EKeys::Zero && !Event.IsRepeat())
			{
				Skip();
			}
			return true;
		}

		virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent&) override { return ShouldBlock(); }
		virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent&) override { return ShouldBlock(); }
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent&) override { return ShouldBlock(); }
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override { return ShouldBlock(); }
		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { return ShouldBlock(); }
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication&, const FPointerEvent&) override { return ShouldBlock(); }
		virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override { return ShouldBlock(); }
		virtual bool HandleMotionDetectedEvent(FSlateApplication&, const FMotionEvent&) override { return ShouldBlock(); }

	private:
		bool ShouldBlock() const
		{
			return Controller.IsValid() && Controller->IsBossSequenceInputBlocked();
		}

		TWeakObjectPtr<ASideScrollingPlayerController> Controller;
		TFunction<void()> Skip;
	};
}


ASideScrollingPlayerController::ASideScrollingPlayerController()
{
	MapWidgetClass = UAreaMapWidget::StaticClass();
}

void ASideScrollingPlayerController::ClientShowLaserPuzzleIntroWidget_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	if (APlayerHUD* LocalHUD = Cast<APlayerHUD>(GetHUD()))
	{
		LocalHUD->ShowLaserPuzzleIntro();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("레이저 퍼즐 UI를 표시할 PlayerHUD가 없습니다."));
	}
}

void ASideScrollingPlayerController::ClientShowCorePuzzleIntroWidget_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	if (APlayerHUD* LocalHUD = Cast<APlayerHUD>(GetHUD()))
	{
		LocalHUD->ShowCorePuzzleIntro();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("코어 퍼즐 UI를 표시할 PlayerHUD가 없습니다."));
	}
}


void ASideScrollingPlayerController::BuildInputStack(TArray<UInputComponent*>& InputStack)
{
	if (bBossSequenceInputBlocked)
	{
		InputStack.Reset();
		return;
	}
	Super::BuildInputStack(InputStack);
}

void ASideScrollingPlayerController::ClientSetBossSequenceInputBlocked_Implementation(
	ABossMonster* SourceBoss, bool bBlocked)
{
	if (!IsLocalController()) return;

	for (auto It = BossSequenceInputSources.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}

	const TWeakObjectPtr<ABossMonster> Source(SourceBoss);
	if (bBlocked && Source.IsValid())
	{
		BossSequenceInputSources.Add(Source);
		ApplyBossSequenceInputBlock(true);
	}
	else
	{
		BossSequenceInputSources.Remove(Source);
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsActorBeingDestroyed()) return;
				for (auto It = BossSequenceInputSources.CreateIterator(); It; ++It)
				{
					if (!It->IsValid()) It.RemoveCurrent();
				}
				ApplyBossSequenceInputBlock(!BossSequenceInputSources.IsEmpty());
			}));
	}
}

void ASideScrollingPlayerController::ApplyBossSequenceInputBlock(bool bBlocked)
{
	if (bBossSequenceInputBlocked == bBlocked) return;

	bBossSequenceInputBlocked = bBlocked;
	FlushPressedKeys();
	SetIgnoreMoveInput(bBlocked);
	SetIgnoreLookInput(bBlocked);

	if (bBlocked)
	{
		if (APlayerBase* ControlledPlayer = Cast<APlayerBase>(GetPawn()))
		{
			ControlledPlayer->CancelWeaponInputsForUI();
			ControlledPlayer->ReleaseGrapplingHook();
			ControlledPlayer->DoMove(0.f);
			ControlledPlayer->DoDrop(0.f);
		}
		if (APawn* PlayerPawn = GetPawn())
		{
			PlayerPawn->ConsumeMovementInputVector();
		}
		if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
		{
			UCharacterMovementComponent* Movement = ControlledCharacter->GetCharacterMovement();
			BossSequenceMovement = Movement;
			SavedBossSequenceMovementMode = Movement->MovementMode;
			SavedBossSequenceCustomMode = Movement->CustomMovementMode;
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}

		if (FSlateApplication::IsInitialized())
		{
			TWeakObjectPtr<ASideScrollingPlayerController> WeakThis(this);
			BossSequenceInputProcessor = MakeShared<FBossSequenceInputProcessor>(this, [WeakThis]()
				{
					if (WeakThis.IsValid()) WeakThis->SkipCinematic();
				});
			FSlateApplication::Get().RegisterInputPreProcessor(BossSequenceInputProcessor, 0);
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
	else
	{
		if (FSlateApplication::IsInitialized() && BossSequenceInputProcessor.IsValid())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(BossSequenceInputProcessor);
		}
		BossSequenceInputProcessor.Reset();

		UCharacterMovementComponent* Movement = BossSequenceMovement.Get();
		ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
		APlayerBase* ControlledPlayer = Cast<APlayerBase>(GetPawn());
		if (IsValid(Movement) && IsValid(ControlledCharacter)
			&& ControlledCharacter->GetCharacterMovement() == Movement
			&& (!IsValid(ControlledPlayer) || !ControlledPlayer->IsDead()))
		{
			Movement->SetMovementMode(
				static_cast<EMovementMode>(SavedBossSequenceMovementMode),
				SavedBossSequenceCustomMode);
		}
		BossSequenceMovement.Reset();
	}
}

// +추가----
void ASideScrollingPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	UpdateAudioAttenuationListener();
}

void ASideScrollingPlayerController::UpdateAudioAttenuationListener()
{
	if (!IsLocalController()) return;

	APawn* PlayerPawn = GetPawn();
	USceneComponent* PlayerRoot = IsValid(PlayerPawn) ? PlayerPawn->GetRootComponent() : nullptr;
	if (IsValid(PlayerRoot))
	{
		// 컴포넌트에 부착하므로 이동과 리스폰에 별도의 매 프레임 갱신이 필요 없다.
		SetAudioListenerAttenuationOverride(PlayerRoot, FVector::ZeroVector);
	}
	else
	{
		ClearAudioListenerAttenuationOverride();
	}
}
// +--------

EMouseCursor::Type ASideScrollingPlayerController::GetMouseCursor() const
{
	const APlayerHUD* PlayerHUD = Cast<APlayerHUD>(GetHUD());
	if (PlayerHUD && PlayerHUD->ShouldUseGameplayCursor())
	{
		// 인게임 커서는 하나: 훅을 걸 수 있으면 청록(Crosshairs), 아니면 주황(EyeDropper)
		return PlayerHUD->ShouldUseGrappleCursor() ? EMouseCursor::Crosshairs : EMouseCursor::EyeDropper;
	}
	return Super::GetMouseCursor();
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

void ASideScrollingPlayerController::SkipCinematic()
{
	if (IsLocalController() && !UGameplayStatics::IsGamePaused(this)) ServerSkipCinematic();
}

void ASideScrollingPlayerController::ServerSkipCinematic_Implementation()
{
	if (UGameplayStatics::IsGamePaused(this)) return;
	for (TActorIterator<ABossMonster> It(GetWorld()); It; ++It)
	{
		if (It->TrySkipPhaseCinematic(this))
		{
			return;
		}
	}
	for (TActorIterator<AOnceCinematicController> It(GetWorld()); It; ++It)
	{
		if (It->IsPlaying()) It->SkipCinematic(this);
	}
}

void ASideScrollingPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BossSequenceInputSources.Reset();
	ApplyBossSequenceInputBlock(false);
	// +추가----
	ClearAudioListenerAttenuationOverride();
	// +--------
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
	// +추가----
	// 로컬 플레이어가 준비되기 전에 SetPawn이 호출된 경우도 처리한다.
	UpdateAudioAttenuationListener();
	// +--------
	if (IsLocalController()) FactoryCursor::Install(GetWorld()->GetGameViewport());
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
	if (UEnhancedInputComponent* Inputs = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MapAction) Inputs->BindAction(MapAction,ETriggerEvent::Started,this,&ASideScrollingPlayerController::ToggleAreaMap);
		if (PauseMenuAction) Inputs->BindAction(PauseMenuAction,ETriggerEvent::Started,this,&ASideScrollingPlayerController::TogglePauseMenu);
		// The IMC owns the key mapping. Bind each IA_Skip asset once across enabled contexts.
		TSet<const UInputAction*> BoundSkipActions;
		auto BindSkipActions = [&](const TArray<UInputMappingContext*>& Contexts)
		{
			for (const UInputMappingContext* Context : Contexts)
			{
				if (!Context) continue;
				for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
				{
					const UInputAction* Action = Mapping.Action.Get();
					if (Action && Action->GetFName() == FName(TEXT("IA_Skip")) && !BoundSkipActions.Contains(Action))
					{
						Inputs->BindAction(Action, ETriggerEvent::Started, this, &ASideScrollingPlayerController::SkipCinematic);
						BoundSkipActions.Add(Action);
					}
				}
			}
		};
		BindSkipActions(DefaultMappingContexts);
		if (!ShouldUseTouchControls()) BindSkipActions(MobileExcludedMappingContexts);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			auto* KeySettings = GetLocalPlayer()->GetSubsystem<UGameKeySettingsSubsystem>();
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(KeySettings ? KeySettings->GetGameplayContext(CurrentContext) : CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(KeySettings ? KeySettings->GetGameplayContext(CurrentContext) : CurrentContext, 0);
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
	if (PauseMenuWidget) PauseMenuWidget->SetPauseInputAction(PauseMenuAction);
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
		// 재개 직후 클릭 없이도 조준 커서로 돌아오도록 포커스를 뷰포트로 즉시 반환
		if (IsLocalController()) FactoryCursor::FocusGameViewport(GetWorld()->GetGameViewport());
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
