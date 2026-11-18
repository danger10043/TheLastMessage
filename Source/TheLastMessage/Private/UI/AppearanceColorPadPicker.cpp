#include "UI/AppearanceColorPadPicker.h"
#include "UI/FactoryMenuStyle.h"
#include "UI/FactoryUIStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "UI/GamepadUIKeys.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

namespace
{
UTextBlock* MakeLabel(UWidgetTree* Tree, const FText& Caption, int32 Size, const FLinearColor& Color)
{
	auto* Label = Tree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Label->GetFont();
	if (UFont* Body = GetDefault<UFactoryUIAssetSettings>()->BodyFont.LoadSynchronous()) Font.FontObject = Body;
	Font.Size = Size;
	Label->SetFont(Font);
	Label->SetText(Caption);
	Label->SetColorAndOpacity(Color);
	return Label;
}

// 설정 화면 슬라이더(StyleSettingsSlider)와 같은 모양.
void StyleSlider(USlider* Slider)
{
	FSliderStyle Style = Slider->GetWidgetStyle();
	Style.NormalBarImage = FactoryUI::Plate(FactoryUI::Panel, FactoryUI::Edge, 2.f);
	Style.HoveredBarImage = FactoryUI::Plate(FactoryUI::Panel, FactoryUI::Cyan, 2.f);
	Style.DisabledBarImage = FactoryUI::Plate(FactoryUI::Panel, FactoryUI::TextMuted, 2.f);
	Style.BarThickness = 6.f;
	auto Thumb = [](const FLinearColor& Fill, const FLinearColor& Edge)
	{
		auto Brush = FactoryUI::Plate(Fill, Edge, 3.f);
		Brush.ImageSize = FVector2D(12.f, 18.f);
		return Brush;
	};
	Style.NormalThumbImage = Thumb(FactoryUI::Cyan, FactoryUI::Text);
	Style.HoveredThumbImage = Thumb(FactoryUI::Amber, FactoryUI::Text);
	Style.DisabledThumbImage = Thumb(FactoryUI::TextMuted, FactoryUI::Edge);
	Slider->SetWidgetStyle(Style);
	Slider->SetSliderBarColor(FLinearColor::White);
	Slider->SetSliderHandleColor(FLinearColor::White);
}

// HSV는 화면에 보이는 sRGB 값 기준으로 계산해야 밝기 슬라이더가 고르게 움직인다.
FLinearColor ToDisplayHSV(const FLinearColor& Linear)
{
	const FColor Srgb = Linear.GetClamped().ToFColorSRGB();
	return FLinearColor(Srgb.R / 255.f, Srgb.G / 255.f, Srgb.B / 255.f).LinearRGBToHSV();
}

FLinearColor FromDisplayHSV(float Hue, float Saturation, float Value)
{
	const FLinearColor Srgb = FLinearColor(Hue, Saturation, Value).HSVToLinearRGB();
	return FLinearColor::FromSRGBColor(Srgb.GetClamped().ToFColor(false));
}
}

void UAppearanceColorPadPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!WidgetTree->RootWidget) BuildTree();
}

TSharedRef<SWidget> UAppearanceColorPadPicker::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
	return FactoryMenuStyle::WithMenuFocusStyle(Super::RebuildWidget());
}

void UAppearanceColorPadPicker::BuildTree()
{
	// 화면 전체를 어둡게 덮고 가운데에 창을 띄운다.
	auto* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PadColorShade"));
	Shade->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Shade->SetHorizontalAlignment(HAlign_Center);
	Shade->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Shade;

	auto* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(620.f);
	Shade->SetContent(Width);
	auto* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrush(FactoryUI::Chamfer(FactoryUI::EPlate::Screen));
	Frame->SetPadding(FMargin(36.f, 28.f));
	Width->AddChild(Frame);
	auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Frame->SetContent(Column);

	TitleText = MakeLabel(WidgetTree, FText::GetEmpty(), 24, FactoryUI::Cyan);
	Column->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

	// 미리보기 색 + 16진수 값.
	auto* PreviewRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(PreviewRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	auto* SwatchSize = WidgetTree->ConstructWidget<USizeBox>();
	SwatchSize->SetWidthOverride(96.f);
	SwatchSize->SetHeightOverride(52.f);
	PreviewRow->AddChildToHorizontalBox(SwatchSize);
	Swatch = WidgetTree->ConstructWidget<UBorder>();
	Swatch->SetBrush(FactoryUI::Plate(FLinearColor::White, FactoryUI::Edge, 3.f));
	SwatchSize->AddChild(Swatch);
	HexText = MakeLabel(WidgetTree, FText::GetEmpty(), 20, FactoryUI::Text);
	auto* HexSlot = PreviewRow->AddChildToHorizontalBox(HexText);
	HexSlot->SetVerticalAlignment(VAlign_Center);
	HexSlot->SetPadding(FMargin(16.f, 0.f));

	HueSlider = AddSliderRow(Column, NSLOCTEXT("PadColorPicker", "Hue", "색상"), 1.f / 72.f);
	SaturationSlider = AddSliderRow(Column, NSLOCTEXT("PadColorPicker", "Saturation", "채도"), 0.02f);
	ValueSlider = AddSliderRow(Column, NSLOCTEXT("PadColorPicker", "Value", "밝기"), 0.02f);

	// 확인 / 취소 버튼.
	auto* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.f, 16.f, 0.f, 8.f));
	auto AddButton = [&](FName Name, const FText& Caption, FactoryUI::ESound Sound, float LeftPadding)
	{
		auto* Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(48.f);
		auto* ButtonSlot = Buttons->AddChildToHorizontalBox(Height);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ButtonSlot->SetPadding(FMargin(LeftPadding, 0.f, 0.f, 0.f));
		auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(FactoryUI::Button());
		Button->SetBackgroundColor(FLinearColor::White);
		FactoryUI::ApplyButtonSounds(Button, Sound);
		Height->AddChild(Button);
		auto* Text = MakeLabel(WidgetTree, Caption, 20, FactoryUI::Text);
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* TextSlot = CastChecked<UButtonSlot>(Button->AddChild(Text));
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		return Button;
	};
	ConfirmButton = AddButton(TEXT("PadColorConfirm"), NSLOCTEXT("PadColorPicker", "Confirm", "확인"), FactoryUI::ESound::Confirm, 0.f);
	CancelButton = AddButton(TEXT("PadColorCancel"), NSLOCTEXT("PadColorPicker", "Cancel", "취소"), FactoryUI::ESound::Cancel, 16.f);
	ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::Confirm);
	CancelButton->OnClicked.AddDynamic(this, &ThisClass::Cancel);

	Column->AddChildToVerticalBox(MakeLabel(WidgetTree,
		NSLOCTEXT("PadColorPicker", "Hint", "십자키 ↑↓ 항목 · ←→ 조절 · A 확인 · B 취소"), 15, FactoryUI::TextMuted));
}

USlider* UAppearanceColorPadPicker::AddSliderRow(UVerticalBox* Box, const FText& Caption, float StepSize)
{
	auto* Height = WidgetTree->ConstructWidget<USizeBox>();
	Height->SetHeightOverride(44.f);
	Box->AddChildToVerticalBox(Height)->SetPadding(FMargin(0.f, 2.f));
	auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Height->AddChild(Row);
	auto* LabelWidth = WidgetTree->ConstructWidget<USizeBox>();
	LabelWidth->SetWidthOverride(120.f);
	auto* Label = MakeLabel(WidgetTree, Caption, 19, FactoryUI::Text);
	Label->SetRenderTranslation(FVector2D(0.f, -5.f));
	LabelWidth->AddChild(Label);
	Row->AddChildToHorizontalBox(LabelWidth)->SetVerticalAlignment(VAlign_Center);
	RowLabels.Add(Label);
	auto* Slider = WidgetTree->ConstructWidget<USlider>();
	StyleSlider(Slider);
	Slider->SetMinValue(0.f);
	Slider->SetMaxValue(1.f);
	Slider->SetStepSize(StepSize);
	Slider->IsFocusable = true;
	Slider->RequiresControllerLock = false;
	Slider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderChanged);
	auto* SliderSlot = Row->AddChildToHorizontalBox(Slider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetVerticalAlignment(VAlign_Center);
	return Slider;
}

void UAppearanceColorPadPicker::Setup(const FText& Title, FLinearColor InitialColor)
{
	if (!WidgetTree->RootWidget) BuildTree();
	TitleText->SetText(Title);
	const FLinearColor HSV = ToDisplayHSV(InitialColor);
	HueSlider->SetValue(HSV.R / 360.f);
	SaturationSlider->SetValue(HSV.G);
	ValueSlider->SetValue(HSV.B);
	RefreshPreview();
}

void UAppearanceColorPadPicker::NativeConstruct()
{
	Super::NativeConstruct();
	if (HueSlider) HueSlider->SetKeyboardFocus();
}

FLinearColor UAppearanceColorPadPicker::GetColor() const
{
	return FromDisplayHSV(HueSlider->GetValue() * 360.f, SaturationSlider->GetValue(), ValueSlider->GetValue());
}

void UAppearanceColorPadPicker::RefreshPreview()
{
	const FLinearColor Color = GetColor();
	Swatch->SetBrushColor(Color);
	HexText->SetText(FText::FromString(TEXT("#") + Color.ToFColorSRGB().ToHex().Left(6)));
}

void UAppearanceColorPadPicker::HandleSliderChanged(float Value)
{
	RefreshPreview();
	OnColorChanged.ExecuteIfBound(GetColor());
}

void UAppearanceColorPadPicker::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	FactoryMenuStyle::UpdateFocus(WidgetTree);
	// 포커스를 잃으면(마우스로 빈 곳을 누른 경우 등) 첫 슬라이더로 되돌린다.
	if (!bFinished && !HasAnyUserFocus() && !HasFocusedDescendants() && HueSlider) HueSlider->SetKeyboardFocus();
	const USlider* Sliders[] = {HueSlider, SaturationSlider, ValueSlider};
	for (int32 Index = 0; Index < RowLabels.Num() && Index < UE_ARRAY_COUNT(Sliders); ++Index)
		if (RowLabels[Index] && Sliders[Index])
			RowLabels[Index]->SetColorAndOpacity(Sliders[Index]->HasAnyUserFocus() ? FactoryUI::Cyan : FactoryUI::Text);
}

FReply UAppearanceColorPadPicker::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || GamepadUI::IsBack(Key))
	{
		if (!Event.IsRepeat())
		{
			FactoryUI::PlayUISound(FactoryUI::ESound::Cancel);
			Cancel();
		}
		return FReply::Handled();
	}
	// 슬라이더에서 A(Enter)를 누르면 바로 확인한다. 버튼에 포커스가 있으면 버튼이 처리한다.
	const bool bButtonFocused = (ConfirmButton && ConfirmButton->HasAnyUserFocus()) || (CancelButton && CancelButton->HasAnyUserFocus());
	if ((Key == EKeys::Enter || GamepadUI::IsAccept(Key)) && !bButtonFocused)
	{
		if (!Event.IsRepeat())
		{
			FactoryUI::PlayUISound(FactoryUI::ESound::Confirm);
			Confirm();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UAppearanceColorPadPicker::Confirm() { Finish(true); }
void UAppearanceColorPadPicker::Cancel() { Finish(false); }

void UAppearanceColorPadPicker::Finish(bool bAccepted)
{
	if (bFinished) return;
	bFinished = true;
	const FLinearColor Color = GetColor();
	RemoveFromParent();
	OnClosed.ExecuteIfBound(bAccepted, Color);
}
