#include "UI/OwnedCoreViewerSubsystem.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/InputComponent.h"
#include "Core/CoreComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/CombatAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "UI/OwnedCoreWidget.h"

UOwnedCoreViewerSubsystem::UOwnedCoreViewerSubsystem()
{
	ToggleKey = EKeys::I;
	ViewerClass = TSoftClassPtr<UOwnedCoreWidget>(FSoftObjectPath(TEXT("/Game/Test_Jihun/Core/WBP/WBP_OwnedCore.WBP_OwnedCore_C")));
}

void UOwnedCoreViewerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (APlayerController* Controller = GetLocalPlayer()->PlayerController) PlayerControllerChanged(Controller);
}

void UOwnedCoreViewerSubsystem::PlayerControllerChanged(APlayerController* Controller)
{
	if (InputController == Controller) return;
	DetachController();
	Super::PlayerControllerChanged(Controller);
	if (!IsValid(Controller) || !Controller->IsLocalController()) return;
	InputController = Controller;
	// 전용 입력은 Pawn이 교체되어도 로컬 플레이어에 유지된다.
	MenuInput = NewObject<UInputComponent>(Controller, NAME_None, RF_Transient);
	MenuInput->Priority = 10;
	MenuInput->RegisterComponent();
	MenuInput->BindKey(ToggleKey, IE_Pressed, this, &ThisClass::ToggleViewer);
	Controller->PushInputComponent(MenuInput);
	Controller->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePawnChanged);
}

bool UOwnedCoreViewerSubsystem::IsViewerOpen() const
{
	return IsValid(Viewer) && Viewer->IsInViewport();
}

bool UOwnedCoreViewerSubsystem::OpenViewer()
{
	APlayerController* Controller = InputController.Get();
	if (!IsValid(Controller) || IsViewerOpen() || UGameplayStatics::IsGamePaused(Controller)) return false;
	UWorld* World = Controller->GetWorld();
	APawn* Pawn = Controller->GetPawn();
	if (!World || World->bIsTearingDown || !IsValid(Pawn)) return false;
	// UIOnly로 열린 다른 화면의 입력과 포커스를 가져오지 않는다.
	if (World->GetGameViewport() && World->GetGameViewport()->IgnoreInput()) return false;
	UCoreComponent* Core = Pawn->FindComponentByClass<UCoreComponent>();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!Core || !ASC) return false;
	if (ASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetHealthAttribute())
		&& ASC->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()) <= 0.f) return false;
	UClass* Class = ViewerClass.LoadSynchronous();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract)) return false;
	UOwnedCoreWidget* Screen = CreateWidget<UOwnedCoreWidget>(Controller, Class);
	if (!Screen || !Screen->SetSources(Core, ASC, ToggleKey)) return false;
	Viewer = Screen;
	bOwnsPause = Controller->SetPause(true, FCanUnpause::CreateUObject(this, &ThisClass::CanUnpause));
	if (!bOwnsPause) { Screen->ReleaseSources(); Viewer = nullptr; return false; }
	InputWorld = World;
	PreviousInput.Capture(*Controller);
	SourcePawn = Pawn;
	Pawn->OnEndPlay.AddDynamic(this, &ThisClass::HandleSourceEndPlay);
	Screen->OnClosed.AddUObject(this, &ThisClass::HandleClosed);
	Screen->AddToPlayerScreen(110);
	if (!Screen->IsInViewport()) { Screen->ReleaseSources(); HandleClosed(); return false; }
	if (APlayerBase* Player = Cast<APlayerBase>(Pawn)) Player->CancelWeaponInputsForUI();
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Screen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(Mode);
	Controller->SetShowMouseCursor(true);
	Controller->FlushPressedKeys();
	Screen->SetKeyboardFocus();
	return true;
}

void UOwnedCoreViewerSubsystem::ToggleViewer()
{
	if (IsViewerOpen()) CloseViewer();
	else OpenViewer();
}

bool UOwnedCoreViewerSubsystem::CanUnpause() const
{
	return !bOwnsPause;
}

void UOwnedCoreViewerSubsystem::CloseViewer()
{
	if (Viewer) Viewer->RemoveFromParent();
}

void UOwnedCoreViewerSubsystem::HandleClosed()
{
	if (SourcePawn.IsValid()) SourcePawn->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleSourceEndPlay);
	SourcePawn.Reset();
	Viewer = nullptr;
	const bool bResume = bOwnsPause;
	bOwnsPause = false;
	APlayerController* Controller = InputController.Get();
	UWorld* World = InputWorld.Get();
	InputWorld.Reset();
	if (!World || World->bIsTearingDown) return;
	if (bResume)
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode()) GameMode->ClearPause();
	}
	// 맵 전환 뒤 새 월드의 입력이나 일시정지를 변경하지 않는다.
	if (!IsValid(Controller) || Controller->GetWorld() != World) return;
	PreviousInput.Restore(*Controller);
}

void UOwnedCoreViewerSubsystem::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	CloseViewer();
}

void UOwnedCoreViewerSubsystem::HandleSourceEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	CloseViewer();
}

void UOwnedCoreViewerSubsystem::DetachController()
{
	CloseViewer();
	if (APlayerController* Controller = InputController.Get())
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
		if (MenuInput) Controller->PopInputComponent(MenuInput);
	}
	if (MenuInput) MenuInput->DestroyComponent();
	MenuInput = nullptr;
	InputController.Reset();
}

void UOwnedCoreViewerSubsystem::Deinitialize()
{
	DetachController();
	Super::Deinitialize();
}
