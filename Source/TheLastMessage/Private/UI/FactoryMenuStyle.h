#pragma once

#include "CoreMinimal.h"

class UWidgetTree;
class UWidgetSwitcher;
class UVerticalBox;
class UTextBlock;

/** Shared presentation for the editable start/pause Widget Blueprints. */
namespace FactoryMenuStyle
{
	UWidgetSwitcher* BuildShell(UWidgetTree* Tree, bool bPause);
	UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, FName Name, const FText& Label, int32 Size);
	void AddButton(UWidgetTree* Tree, UVerticalBox* Box, FName Name, FName TextName, const FText& Label, bool bCompact = false);
	void UpdateFocus(UWidgetTree* Tree);
}
