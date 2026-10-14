#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SideScrollingPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

UCLASS(Config=Game)
class THELASTMESSAGE_API ASideScrollingPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ASideScrollingPlayerController();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> SideScrollingMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MouseMappingContext;
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TSubclassOf<UUserWidget> TouchControlsWidgetClass;
	UPROPERTY(EditAnywhere, Config, Category="Input")
	bool bForceTouchControls = false;
private:
	void InitializeLocalInput();
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TouchControlsWidget;
	UPROPERTY(Transient)
	TSubclassOf<APawn> RespawnCharacterClass;
	TWeakObjectPtr<APawn> ObservedPawn;
	FTimerHandle InputInitializationTimer;
};
