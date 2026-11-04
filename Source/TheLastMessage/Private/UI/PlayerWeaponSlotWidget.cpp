#include "UI/PlayerWeaponSlotWidget.h"
#include "UI/FactoryUIStyle.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UPlayerWeaponSlotWidget::SetWeaponPresentation(UWeaponDataAsset* Data, int32 Number, bool bOwned, EWeaponSlot Assignment, bool bSelected)
{
	if (KeyLabel) KeyLabel->SetText(FText::AsNumber(Number));
	if (WeaponIcon)
	{
		WeaponIcon->SetBrushFromTexture(bOwned && Data ? Data->WeaponIcon.Get() : nullptr);
		WeaponIcon->SetVisibility(bOwned && Data && Data->WeaponIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (LeftMouseIcon) LeftMouseIcon->SetVisibility(bOwned && Assignment == EWeaponSlot::Primary ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (RightMouseIcon) RightMouseIcon->SetVisibility(bOwned && Assignment == EWeaponSlot::Secondary ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (SelectionFrame)
	{
		const bool bActive = bSelected && bOwned;
		if (!StyledSelection.IsSet() || StyledSelection.GetValue() != bActive)
		{
			StyledSelection = bActive;
			SelectionFrame->SetBrush(FactoryUI::Chamfer(bActive ? FactoryUI::EPlate::Selected : FactoryUI::EPlate::Normal));
			SelectionFrame->SetBrushColor(FLinearColor::White);
			if (KeyLabel) KeyLabel->SetColorAndOpacity(bActive ? FactoryUI::Amber : FactoryUI::Text);
		}
	}
}
