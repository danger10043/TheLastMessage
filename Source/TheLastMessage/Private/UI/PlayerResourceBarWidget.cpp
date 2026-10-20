#include "UI/PlayerResourceBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerResourceBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (ValueBar) ValueBar->SetFillColorAndOpacity(FillColor);
	SetResourceValues(CurrentValue, MaximumValue);
}

void UPlayerResourceBarWidget::SetResourceValues(float Current, float Maximum)
{
	CurrentValue = FMath::IsFinite(Current) ? FMath::Max(0.0f, Current) : 0.0f;
	MaximumValue = FMath::IsFinite(Maximum) ? FMath::Max(0.0f, Maximum) : 0.0f;
	if (ValueBar) ValueBar->SetPercent(MaximumValue > 0 ? FMath::Clamp(CurrentValue / MaximumValue, 0.0f, 1.0f) : 0.0f);
	if (ValueText) ValueText->SetText(FText::Format(NSLOCTEXT("PlayerUI", "CompactResourceCount", "{0} / {1}"),
		FText::AsNumber(FMath::CeilToInt(CurrentValue)), FText::AsNumber(FMath::CeilToInt(MaximumValue))));
}
