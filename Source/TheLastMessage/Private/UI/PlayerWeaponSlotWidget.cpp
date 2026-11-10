#include "UI/PlayerWeaponSlotWidget.h"
#include "UI/FactoryUIStyle.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture2D.h"

namespace
{
// HUD silhouettes authored for the slot's icon area (75 x 62, texture 256 x 212).
// Source PNGs: SourceArt/UI/HUD/WeaponIcons
UTexture2D* HudWeaponIcon(EWeaponType Type)
{
	static TWeakObjectPtr<UTexture2D> Cache[4];
	const TCHAR* Paths[4] = {
		TEXT("/Game/Main/UI/HUD/Textures/WeaponIcons/T_UI_WeaponIcon_Pipe"),
		TEXT("/Game/Main/UI/HUD/Textures/WeaponIcons/T_UI_WeaponIcon_Hammer"),
		TEXT("/Game/Main/UI/HUD/Textures/WeaponIcons/T_UI_WeaponIcon_Pistol"),
		TEXT("/Game/Main/UI/HUD/Textures/WeaponIcons/T_UI_WeaponIcon_Railgun") };
	int32 Index = INDEX_NONE;
	switch (Type)
	{
	case EWeaponType::LightMelee: Index = 0; break;
	case EWeaponType::HeavyMelee: Index = 1; break;
	case EWeaponType::LightRanged: Index = 2; break;
	case EWeaponType::HeavyRanged: Index = 3; break;
	default: return nullptr;
	}
	if (!Cache[Index].IsValid()) Cache[Index] = LoadObject<UTexture2D>(nullptr, Paths[Index], nullptr, LOAD_NoWarn);
	return Cache[Index].Get();
}
}

void UPlayerWeaponSlotWidget::SetWeaponPresentation(UWeaponDataAsset* Data, int32 Number, bool bOwned, EWeaponSlot Assignment, bool bSelected)
{
	PresentedType = bOwned && Data ? TOptional<EWeaponType>(Data->WeaponType) : TOptional<EWeaponType>();
	if (KeyLabel) KeyLabel->SetText(FText::AsNumber(Number));
	if (WeaponIcon)
	{
		// Texture icon first (HUD set, then the data asset's icon); vector silhouette in NativePaint is the fallback.
		UTexture2D* Icon = PresentedType.IsSet() ? HudWeaponIcon(PresentedType.GetValue()) : nullptr;
		if (!Icon && PresentedType.IsSet()) Icon = Data->WeaponIcon;
		if (WeaponIcon->GetBrush().GetResourceObject() != Icon)
		{
			if (Icon) WeaponIcon->SetBrushFromTexture(Icon, false);
			else WeaponIcon->SetBrushResourceObject(nullptr);
		}
		const FLinearColor IconTint = !Icon ? FLinearColor::Transparent
			: (bSelected ? FMath::Lerp(FactoryUI::Text, FactoryUI::Amber, 0.3f) : FactoryUI::Text);
		WeaponIcon->SetColorAndOpacity(IconTint);
		WeaponIcon->SetVisibility(PresentedType.IsSet() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
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

int32 UPlayerWeaponSlotWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Out, Layer, Style, bEnabled);
	if (!PresentedType.IsSet() || !WeaponIcon) return Layer;
	if (WeaponIcon->GetBrush().GetResourceObject()) return Layer;   // texture icon is drawn by the Image itself
	const FGeometry& Icon = WeaponIcon->GetCachedGeometry();
	const FVector2D Size = Icon.GetLocalSize();
	// Every silhouette uses the same 100 x 56 canvas, with identical margins.
	const float Scale = FMath::Min(Size.X / 100.f, Size.Y / 56.f) * 0.86f;
	const FVector2D Offset = (Size - FVector2D(100, 56) * Scale) * 0.5f;
	const FLinearColor Color = FLinearColor(0.94f, 0.91f, 0.82f) * Style.GetColorAndOpacityTint();
	auto Line = [&](std::initializer_list<FVector2D> Points, float Width = 4.f)
	{
		TArray<FVector2D> Path;
		for (const FVector2D& P : Points) Path.Add(Offset + P * Scale);
		FSlateDrawElement::MakeLines(Out, Layer + 1, Icon.ToPaintGeometry(), Path, ESlateDrawEffect::None, Color, true, Width * Scale);
	};
	switch (PresentedType.GetValue())
	{
	case EWeaponType::LightMelee:
		Line({{12,44},{69,12},{85,12},{85,19}}, 7); Line({{18,36},{26,47}}, 3); break;
	case EWeaponType::HeavyMelee:
		Line({{15,46},{61,18}}, 7); Line({{48,10},{62,5},{85,24},{70,35},{48,10}}, 5); break;
	case EWeaponType::LightRanged:
		Line({{12,17},{85,17},{85,29},{45,29},{38,48},{23,48},{30,29},{12,29},{12,17}}, 4);
		Line({{48,30},{48,38},{40,38}}, 3); Line({{70,12},{76,12}}, 3); break;
	case EWeaponType::HeavyRanged:
		Line({{8,20},{20,14},{62,14},{68,20},{91,20},{91,29},{66,29},{59,36},{40,36},{35,48},{22,48},{27,33},{8,33},{8,20}}, 4);
		Line({{40,14},{40,7},{62,7},{62,14}}, 3); Line({{50,22},{67,22}}, 3); break;
	}
	return Layer + 1;
}
