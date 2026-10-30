#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWidgetSwitcher;
class UWidgetTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMenuSlotSelected, int32, SlotId, bool, bNewGame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMenuExitRequested);

/** Presentation only. Selecting a slot never changes or writes a save file. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static void BuildDefaultLayout(UWidgetTree* Tree);
	static bool AddConfirmationLayout(UWidgetTree* Tree);
	UFUNCTION(BlueprintCallable, Category="Menu") void ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category="Menu") void ShowNewGameSlots();
	UFUNCTION(BlueprintCallable, Category="Menu") void ShowLoadGameSlots();
	/** Supply real slot metadata later; no filesystem access in this widget. */
	UFUNCTION(BlueprintCallable, Category="Menu") void SetSlotPresentation(int32 SlotId, bool bOccupied, FText Description, bool bCanLoad = true);
	void SetBusy(bool bBusy);
	void ShowError(const FText& Message);
	UPROPERTY(BlueprintAssignable, Category="Menu") FMenuSlotSelected OnSlotSelected;
	UPROPERTY(BlueprintAssignable, Category="Menu") FMenuExitRequested OnExitRequested;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> NewGameButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> LoadGameButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ExitButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Slot1Button;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Slot2Button;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Slot3Button;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SlotHeading;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Slot1Text;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Slot2Text;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Slot3Text;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SelectionText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> CancelButton;
private:
	void RefreshSlots();
	void SelectSlot(int32 SlotId);
	UFUNCTION() void SelectSlot1();
	UFUNCTION() void SelectSlot2();
	UFUNCTION() void SelectSlot3();
	UFUNCTION() void RequestExit();
	UFUNCTION() void ConfirmOverwrite();
	UFUNCTION() void CancelOverwrite();
	bool bNewGame = true;
	bool bWorking = false;
	int32 PendingSlot = 0;
	bool Occupied[3] = {false, false, false};
	bool Loadable[3] = {false, false, false};
	FText Descriptions[3];
};
