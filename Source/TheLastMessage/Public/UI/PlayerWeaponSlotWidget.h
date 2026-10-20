#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Weapon/WeaponTypes.h"
#include "PlayerWeaponSlotWidget.generated.h"

class UWeaponDataAsset;
class UImage;
class UTextBlock;
class UBorder;

/** Reusable, designer-editable weapon card. Unowned entries retain an empty frame. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UPlayerWeaponSlotWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Player UI")
	void SetWeaponPresentation(UWeaponDataAsset* Data, int32 Number, bool bOwned, EWeaponSlot Assignment, bool bSelected);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") FLinearColor SelectedColor = FLinearColor(0.1f, 0.7f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") FLinearColor FrameColor = FLinearColor(0.035f, 0.04f, 0.05f);
protected:
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UImage> WeaponIcon;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UImage> LeftMouseIcon;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UImage> RightMouseIcon;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UTextBlock> KeyLabel;
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly) TObjectPtr<UBorder> SelectionFrame;
};
