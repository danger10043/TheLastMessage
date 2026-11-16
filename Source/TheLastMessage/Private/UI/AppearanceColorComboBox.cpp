#include "AppearanceColorComboBox.h"
#include "Widgets/Layout/SBox.h"

void UAppearanceColorComboBox::CopyStyleFrom(const UComboBoxString& Source)
{
	SetWidgetStyle(Source.GetWidgetStyle());
	SetItemStyle(Source.GetItemStyle());
	SetContentPadding(Source.GetContentPadding());
	SetMaxListHeight(Source.GetMaxListHeight());
	SetHasDownArrow(Source.IsHasDownArrow());
	SetEnableGamepadNavigationMode(Source.IsEnableGamepadNavigationMode());
	InitScrollBarStyle(Source.GetScrollBarStyle());
	InitFont(Source.GetFont());
	InitIsFocusable(Source.IsFocusable());
	InitForegroundColor(Source.GetForegroundColor());
	SetToolTipText(Source.GetToolTipText());
}

TSharedRef<SWidget> UAppearanceColorComboBox::RebuildWidget()
{
	using SColorCombo = SComboBox<TSharedPtr<FString>>;
	const int32 SelectedIndex = GetSelectedIndex();
	CurrentOptionPtr = Options.IsValidIndex(SelectedIndex) ? Options[SelectedIndex] : nullptr;
	MyComboBox = SNew(SColorCombo)
		.ComboBoxStyle(&GetWidgetStyle())
		.ItemStyle(&GetItemStyle())
		.ScrollBarStyle(&GetScrollBarStyle())
		.ForegroundColor(GetForegroundColor())
		.OptionsSource(&Options)
		.InitiallySelectedItem(CurrentOptionPtr)
		.ContentPadding(GetContentPadding())
		.MaxListHeight(GetMaxListHeight())
		.HasDownArrow(IsHasDownArrow())
		.EnableGamepadNavigationMode(IsEnableGamepadNavigationMode())
		.IsFocusable(IsFocusable())
		.ReselectionTriggersOnSelectionChanged(true)
		.OnGenerateWidget(BIND_UOBJECT_DELEGATE(SColorCombo::FOnGenerateWidget, HandleGenerateWidget))
		.OnSelectionChanged(BIND_UOBJECT_DELEGATE(SColorCombo::FOnSelectionChanged, HandleSelectionChanged))
		.OnComboBoxOpening(BIND_UOBJECT_DELEGATE(FOnComboBoxOpening, HandleOpening))
		[
			SAssignNew(ComboBoxContent, SBox)
		];
	UpdateOrGenerateWidget(CurrentOptionPtr);
	return MyComboBox.ToSharedRef();
}
