#include "VolumeSlider.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SSlider.h"

namespace
{
class SVolumeSlider : public SSlider
{
public:
	FSimpleDelegate OnToggleMute;
	FVector2D ThumbSize;

	FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2D Point = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		const double ThumbCenter = ThumbSize.X * 0.5 + GetNormalizedValue() * (Size.X - ThumbSize.X);
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton
			&& FMath::Abs(Point.X - ThumbCenter) <= ThumbSize.X * 0.5
			&& FMath::Abs(Point.Y - Size.Y * 0.5) <= ThumbSize.Y * 0.5)
		{
			PressPosition = Event.GetScreenSpacePosition();
			CachedCursor = GetCursor().Get(EMouseCursor::Default);
			bThumbPressed = true;
			return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
		}
		return SSlider::OnMouseButtonDown(Geometry, Event);
	}

	FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (bThumbPressed)
		{
			if (FVector2D::DistSquared(PressPosition, Event.GetScreenSpacePosition()) < FMath::Square(4.0))
				return FReply::Handled();
			bThumbPressed = false;
		}
		return SSlider::OnMouseMove(Geometry, Event);
	}

	FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (bThumbPressed && Event.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bThumbPressed = false;
			OnToggleMute.ExecuteIfBound();
			return FReply::Handled().ReleaseMouseCapture();
		}
		return SSlider::OnMouseButtonUp(Geometry, Event);
	}

	void OnMouseCaptureLost(const FCaptureLostEvent& Event) override
	{
		bThumbPressed = false;
		SSlider::OnMouseCaptureLost(Event);
	}

private:
	FVector2D PressPosition;
	bool bThumbPressed = false;
};
}

TSharedRef<SWidget> UVolumeSlider::RebuildWidget()
{
	const auto Slider = SNew(SVolumeSlider)
		.Style(&GetWidgetStyle())
		.IsFocusable(IsFocusable)
		.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnValueChanged));
	Slider->ThumbSize = GetWidgetStyle().NormalThumbImage.ImageSize;
	Slider->OnToggleMute.BindUObject(this, &UVolumeSlider::ToggleMute);
	MySlider = Slider;
	return Slider;
}
