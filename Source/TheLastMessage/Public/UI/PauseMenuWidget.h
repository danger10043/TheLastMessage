#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UWidgetTree;
class UWidgetSwitcher;
class UTextBlock;

UENUM(BlueprintType)
enum class EPauseMenuAction : uint8 { Resume, ReloadCheckpoint, MainMenu, QuitGame };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPauseMenuActionRequested, EPauseMenuAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPauseMenuRemoved);

UCLASS(Blueprintable)
class THELASTMESSAGE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static void BuildDefaultLayout(UWidgetTree* Tree);
	void SetCheckpointAvailable(bool bAvailable);
	void SetBusy(bool bBusy);
	void ShowError(const FText& Message);
	UPROPERTY(BlueprintAssignable, Category="Pause Menu") FPauseMenuActionRequested OnActionRequested;
	FPauseMenuRemoved OnMenuRemoved;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	UFUNCTION() void Resume();
	UFUNCTION() void Reload();
	UFUNCTION() void OpenSettings();
	UFUNCTION() void MainMenu();
	UFUNCTION() void Quit();
	UFUNCTION() void Confirm();
	UFUNCTION() void Back();
	void Ask(EPauseMenuAction Action, const FText& Message);
	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> Pages;
	bool bWorking = false;
	bool bCheckpointAvailable = false;
	EPauseMenuAction PendingAction = EPauseMenuAction::Resume;
};
