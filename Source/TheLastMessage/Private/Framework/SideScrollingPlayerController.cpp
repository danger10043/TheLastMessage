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

ASideScrollingPlayerController::ASideScrollingPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputAction> Action(TEXT("/Game/Main/Inputs/Actions/IA_Map.IA_Map"));
	MapAction = Action.Object;
	MapWidgetClass = UAreaMapWidget::StaticClass();
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
