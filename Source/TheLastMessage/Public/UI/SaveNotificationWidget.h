#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveNotificationWidget.generated.h"

class UTextBlock;
class UWidgetTree;

UCLASS(Blueprintable)
class THELASTMESSAGE_API USaveNotificationWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static void BuildDefaultLayout(UWidgetTree* Tree);
	UFUNCTION(BlueprintCallable, Category="Save UI") void ShowResult(bool bSuccess, bool bHasSlot = true);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Save UI", meta=(ClampMin="0.1")) float DisplayDuration = 3;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;
private:
	void HideNotification();
	FTimerHandle HideTimer;
};
