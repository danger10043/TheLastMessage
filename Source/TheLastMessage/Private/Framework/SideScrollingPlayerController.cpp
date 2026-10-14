#include "Framework/SideScrollingPlayerController.h"
#include "Camera/SideScrollingCameraManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "TimerManager.h"

ASideScrollingPlayerController::ASideScrollingPlayerController()
{
	PlayerCameraManagerClass = ASideScrollingCameraManager::StaticClass();
	bEnableTouchEvents = true;
}

void ASideScrollingPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Match the BP's one-frame delay for additional local players.
	InputInitializationTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ASideScrollingPlayerController::InitializeLocalInput);
}

void ASideScrollingPlayerController::InitializeLocalInput()
{
	if (!IsLocalPlayerController()) return;
	const bool bTouch = bForceTouchControls || SVirtualJoystick::ShouldDisplayTouchInterface();
	if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (SideScrollingMappingContext) Input->AddMappingContext(SideScrollingMappingContext, 0);
		if (!bTouch && MouseMappingContext) Input->AddMappingContext(MouseMappingContext, 0);
	}
	if (bTouch && TouchControlsWidgetClass)
	{
		TouchControlsWidget = CreateWidget<UUserWidget>(this, TouchControlsWidgetClass);
		if (TouchControlsWidget) TouchControlsWidget->AddToPlayerScreen(0);
	}
}

void ASideScrollingPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (APawn* Previous = ObservedPawn.Get()) Previous->OnDestroyed.RemoveDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
	ObservedPawn = InPawn;
	if (InPawn)
	{
		RespawnCharacterClass = InPawn->GetClass();
		InPawn->OnDestroyed.AddUniqueDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
	}
}

void ASideScrollingPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown || !RespawnCharacterClass) return;
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
	if (Starts.IsEmpty()) return;
	const FTransform SpawnTransform = Starts[FMath::RandRange(0, Starts.Num() - 1)]->GetActorTransform();
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	if (APawn* RespawnedPawn = GetWorld()->SpawnActor<APawn>(RespawnCharacterClass, SpawnTransform, Parameters)) Possess(RespawnedPawn);
}

void ASideScrollingPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InputInitializationTimer);
	if (APawn* PreviousPawn = ObservedPawn.Get()) PreviousPawn->OnDestroyed.RemoveDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
	if (TouchControlsWidget) TouchControlsWidget->RemoveFromParent();
	if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (SideScrollingMappingContext) Input->RemoveMappingContext(SideScrollingMappingContext);
		if (MouseMappingContext) Input->RemoveMappingContext(MouseMappingContext);
	}
	Super::EndPlay(EndPlayReason);
}
