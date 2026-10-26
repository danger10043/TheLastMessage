#include "UI/StartMenuHUD.h"
#include "UI/StartMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"

AStartMenuHUD::AStartMenuHUD()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FClassFinder<UStartMenuWidget> Widget(TEXT("/Game/Main/UI/Menu/WBP_StartMenu"));
	MenuClass = Widget.Succeeded() ? Widget.Class.Get() : UStartMenuWidget::StaticClass();
}

void AStartMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	Menu = CreateWidget<UStartMenuWidget>(PC, MenuClass);
	if (!Menu) return;
	Menu->OnExitRequested.AddUniqueDynamic(this, &AStartMenuHUD::ExitGame);
	Menu->AddToPlayerScreen();
	FInputModeUIOnly Input;
	Input.SetWidgetToFocus(Menu->TakeWidget());
	Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Input);
	PC->SetShowMouseCursor(true);
	Menu->ShowMainMenu();
}

void AStartMenuHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Menu) Menu->RemoveFromParent();
	Menu = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AStartMenuHUD::ExitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

AStartMenuGameMode::AStartMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AStartMenuHUD::StaticClass();
}
