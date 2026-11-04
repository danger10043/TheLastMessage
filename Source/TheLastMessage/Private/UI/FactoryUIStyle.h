#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"

namespace FactoryUI
{
inline const FLinearColor Panel(0.012f, 0.023f, 0.031f, 0.96f);
inline const FLinearColor Edge(0.17f, 0.35f, 0.42f);
inline const FLinearColor Cyan(0.30f, 0.83f, 0.94f);
inline const FLinearColor Amber(1.0f, 0.57f, 0.16f);
inline const FLinearColor SelectedPanel(0.13f, 0.063f, 0.015f, 0.98f);
inline const FLinearColor Text(0.88f, 0.94f, 0.96f);

enum class EPlate : uint8 { Normal, Selected, Screen, Count };
const TCHAR* PlatePath(EPlate State);
void MakePlatePixels(EPlate State, TArray<FColor>& Pixels);
FSlateBrush Chamfer(EPlate State = EPlate::Normal);

inline FSlateRoundedBoxBrush Plate(FLinearColor Fill, FLinearColor Outline, float Radius = 4.0f)
{
	return FSlateRoundedBoxBrush(Fill, Radius, Outline, 1.0f);
}

inline FButtonStyle Button(bool bSelected = false)
{
	FButtonStyle Style;
	Style.Normal = Chamfer(bSelected ? EPlate::Selected : EPlate::Normal);
	Style.Hovered = Chamfer(EPlate::Selected);
	Style.Pressed = Style.Hovered;
	Style.Pressed.TintColor = FLinearColor(0.75f, 0.75f, 0.75f);
	Style.Disabled = Chamfer();
	Style.Disabled.TintColor = FLinearColor(0.5f, 0.5f, 0.5f);
	Style.NormalPadding = FMargin(12, 4);
	Style.PressedPadding = FMargin(12, 5, 12, 3);
	return Style;
}

inline FProgressBarStyle Meter(const FProgressBarStyle& Existing)
{
	FProgressBarStyle Style = Existing;
	Style.BackgroundImage = Plate(Panel, Edge, 2.0f);
	Style.FillImage = FSlateRoundedBoxBrush(FLinearColor::White, 2.0f);
	return Style;
}
}
