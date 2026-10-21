#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/GameCoreTypes.h"
#include "OwnedCoreItemWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnOwnedCoreItemClicked, FName);

/** 보유 코어 하나의 아이콘과 단계를 표시한다. 이름·효과 설명은 부모 화면이 담당한다. */
UCLASS(Abstract)
class THELASTMESSAGE_API UOwnedCoreItemWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetEntry(const FOwnedCoreEntry& Entry);
	void SetSelected(bool bSelected);
	FName GetCoreId() const { return CoreId; }
	FOnOwnedCoreItemClicked OnCoreClicked;

protected:
	virtual void NativeOnInitialized() override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CoreButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CoreIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> SelectionFrame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> LevelPips;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> OverflowLevel;
	UPROPERTY(EditDefaultsOnly, Category = "Core|Appearance") FLinearColor SelectedColor = FLinearColor(0.96f, .48f, .17f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Core|Appearance") FLinearColor FilledPipColor = FLinearColor(0.96f, .48f, .17f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Core|Appearance") FLinearColor EmptyPipColor = FLinearColor(.08f, .12f, .13f, 1.f);

private:
	UFUNCTION() void HandleClicked();
	FName CoreId;
	FSlateBrush DefaultIcon;
};
