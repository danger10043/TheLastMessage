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
		// The Widget Blueprint owns the chamfered meter brushes (T_HUD_MeterTrack / T_HUD_MeterFill).
		// Only fall back to the generated rounded meter when no fill texture was assigned.
		if (!ValueBar->GetWidgetStyle().FillImage.GetResourceObject())
		{
			ValueBar->SetWidgetStyle(FactoryUI::Meter(ValueBar->GetWidgetStyle()));
		}
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

	// Quarter ticks: short notches along the bottom edge only, so they never cut through
	// the label / value glyphs (full-height dividers read like "|" or made "N" look like "PI").
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	constexpr float EdgeInset = 3.0f;              // matches the fill inset in T_HUD_MeterFill
	const float TickHeight = FMath::Max(3.0f, static_cast<float>(Size.Y) * 0.22f);
	for (int32 Segment = 1; Segment < 4; ++Segment)
	{
		const float X = static_cast<float>(Size.X) * Segment / 4.0f - 0.75f;
		FSlateDrawElement::MakeBox(Out, Layer+1, Meter.ToPaintGeometry(
			FVector2f(1.5f, TickHeight), FSlateLayoutTransform(FVector2f(X, static_cast<float>(Size.Y) - EdgeInset - TickHeight))),
			Brush, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	}
	return Layer+1;
}
