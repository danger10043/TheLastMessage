#include "UI/OwnedCoreItemWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DataAsset/CoreDataAsset.h"
#include "Blueprint/WidgetTree.h"

void UOwnedCoreItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CoreButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	DefaultIcon = CoreIcon->GetBrush();
}

void UOwnedCoreItemWidget::SetEntry(const FOwnedCoreEntry& Entry)
{
	const UCoreDataAsset* Data = Entry.CoreData;
	if (!IsValid(Data)) return;
	CoreId = Data->CoreId;
	if (Data->CoreIcon) CoreIcon->SetBrushFromTexture(Data->CoreIcon, false);
	else CoreIcon->SetBrush(DefaultIcon);
	LevelPips->ClearChildren();
	// 많은 단계나 무한 강화는 막대를 잘라 표시하지 않고 숫자로 정확히 표시한다.
	const bool bUsePips = Data->UpgradeLimitType == ECoreUpgradeLimitType::Custom && Data->MaxLevel <= 12;
	LevelPips->SetVisibility(bUsePips && Data->MaxLevel > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	OverflowLevel->SetVisibility(bUsePips ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	OverflowLevel->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), Entry.Level)));
	if (bUsePips && Data->MaxLevel > 1)
	{
		for (int32 Index = 0; Index < Data->MaxLevel; ++Index)
		{
			UImage* Pip = WidgetTree->ConstructWidget<UImage>();
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = FVector2D(7, 4);
			Brush.TintColor = Index < Entry.Level ? FilledPipColor : EmptyPipColor;
			Pip->SetBrush(Brush);
			Pip->SetVisibility(ESlateVisibility::HitTestInvisible);
			LevelPips->AddChildToHorizontalBox(Pip)->SetPadding(FMargin(1, 0));
		}
	}
}

void UOwnedCoreItemWidget::SetSelected(bool bSelected)
{
	SelectionFrame->SetBrushColor(bSelected ? SelectedColor : FLinearColor::Transparent);
}

void UOwnedCoreItemWidget::HandleClicked()
{
	OnCoreClicked.Broadcast(CoreId);
}
