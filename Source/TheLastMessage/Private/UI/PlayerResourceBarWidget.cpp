#include "UI/PlayerResourceBarWidget.h"
#include "UI/FactoryUIStyle.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UPlayerResourceBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (ValueBar)
	{
		ValueBar->SetWidgetStyle(FactoryUI::Meter(ValueBar->GetWidgetStyle()));
		ValueBar->SetFillColorAndOpacity(FillColor);
	}
	if (ValueText) ValueText->SetColorAndOpacity(FactoryUI::Text);
	if (LabelText) LabelText->SetText(ResourceLabel);
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

int32 UPlayerResourceBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Out, Layer, Style, bEnabled);
	if (!ValueBar || !ValueBar->IsVisible()) return Layer;
	const FGeometry& Meter = ValueBar->GetCachedGeometry();
	const FVector2D Size = Meter.GetLocalSize();
	if (Size.X < 20 || Size.Y < 4) return Layer;

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	for (int32 Segment = 1; Segment < 5; ++Segment)
	{
		const float X = Size.X * Segment / 5.0f;
		FSlateDrawElement::MakeBox(Out, Layer+1, Meter.ToPaintGeometry(
			FVector2f(1.5f, Size.Y-2), FSlateLayoutTransform(FVector2f(X, 1))),
			Brush, ESlateDrawEffect::None, FLinearColor(0.007f, 0.013f, 0.018f, 0.85f));
	}
	return Layer+1;
}
