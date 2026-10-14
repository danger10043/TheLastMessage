#include "Framework/SideScrollingGameMode.h"
#include "Framework/SideScrollingPlayerController.h"
#include "Character/Player/RobotPlayerCharacter.h"
#include "Interfaces/SideScrollerInterop.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ASideScrollingGameMode::ASideScrollingGameMode()
{
	DefaultPawnClass = ARobotPlayerCharacter::StaticClass();
	PlayerControllerClass = ASideScrollingPlayerController::StaticClass();
}

void ASideScrollingGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UserInterfaceClass)
	{
		UserInterface = CreateWidget<UUserWidget>(UGameplayStatics::GetPlayerController(this, 0), UserInterfaceClass);
		USideScrollerInterop::UpdatePickupWidget(UserInterface, 0);
	}
	for (int32 Index = 1; Index < FMath::Clamp(NumberOfLocalPlayers, 1, 4); ++Index)
	{
		UGameplayStatics::CreatePlayer(this, -1, true);
	}
}

AActor* ASideScrollingGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> Starts;
	const FName Tag(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment++));
	UGameplayStatics::GetAllActorsOfClassWithTag(this, APlayerStart::StaticClass(), Tag, Starts);
	if (Starts.IsEmpty()) UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), Starts);
	return Starts.IsEmpty() ? Super::ChoosePlayerStart_Implementation(Player) : Starts[0];
}

void ASideScrollingGameMode::ProcessPickup(AActor* Interactor)
{
	if (!IsValid(Interactor) || (PickupClass && !Interactor->IsA(PickupClass))) return;
	++PickupsCollected;
	if (UserInterface)
	{
		USideScrollerInterop::UpdatePickupWidget(UserInterface, PickupsCollected);
		if (PickupsCollected == 1) UserInterface->AddToViewport();
	}
}

void ASideScrollingGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UserInterface) UserInterface->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}
