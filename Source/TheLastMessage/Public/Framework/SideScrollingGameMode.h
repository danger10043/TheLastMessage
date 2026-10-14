#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SideScrollingGameMode.generated.h"

class UUserWidget;

UCLASS()
class THELASTMESSAGE_API ASideScrollingGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ASideScrollingGameMode();
	UFUNCTION(BlueprintCallable, Category="Pickups")
	void ProcessPickup(AActor* Interactor);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> UserInterfaceClass;
	UPROPERTY(EditDefaultsOnly, Category="Pickups")
	TSubclassOf<AActor> PickupClass;
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta=(ClampMin="1", ClampMax="4"))
	int32 NumberOfLocalPlayers = 1;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Pickups")
	int32 PickupsCollected = 0;
private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> UserInterface;
	int32 CurrentPlayerStartAssignment = 0;
};
