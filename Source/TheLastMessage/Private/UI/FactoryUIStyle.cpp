#include "UI/FactoryUIStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "Engine/Texture2D.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Sound/SoundBase.h"

namespace FactoryUI
{
constexpr int32 PlateSize = 64;

const TSoftObjectPtr<UTexture2D>& GetPlateTexture(EPlate State)
{
	const auto* Assets = GetDefault<UFactoryUIAssetSettings>();
	switch (State)
	{
	case EPlate::Selected: return Assets->SelectedPlate;
	case EPlate::Screen: return Assets->ScreenPlate;
	default: return Assets->Plate;
	}
}

void MakePlatePixels(EPlate State, TArray<FColor>& Pixels)
{
	constexpr int32 Size = PlateSize;
	constexpr int32 Corner = 7;
	Pixels.SetNumUninitialized(Size * Size);
	const bool bSelected = State == EPlate::Selected;
	const bool bScreen = State == EPlate::Screen;
	const FLinearColor Outline = bSelected ? FLinearColor(1.0f, 0.66f, 0.25f) : bScreen ? Cyan : FLinearColor(0.12f, 0.18f, 0.21f);
	const FLinearColor Fill = bSelected ? FLinearColor(0.028f, 0.017f, 0.008f, 0.98f) : FLinearColor(0.006f, 0.011f, 0.014f, 0.96f);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const int32 Horizontal = FMath::Min(X, Size - 1 - X);
			const int32 Vertical = FMath::Min(Y, Size - 1 - Y);
			const int32 Distance = FMath::Min3(Horizontal, Vertical, Horizontal + Vertical - Corner);
			FLinearColor Color = FLinearColor::Transparent;
			if (Distance >= 0)
			{
					const float Shine = FMath::Exp(-Vertical * 0.32f) * (bSelected ? 0.095f : 0.035f);
					Color = FMath::Lerp(Fill, Outline, Shine);
				if (Distance == 0) Color = Outline;
					else if (Distance < 5 && bSelected) Color = FMath::Lerp(Color, Outline, (5 - Distance) * 0.035f);
			}
			Pixels[Y * Size + X] = Color.ToFColorSRGB();
		}
	}
}

FSlateBrush Chamfer(EPlate State)
{
	constexpr int32 Size = PlateSize;
	static TWeakObjectPtr<UTexture2D> Textures[static_cast<int32>(EPlate::Count)];
	auto& Cached = Textures[static_cast<int32>(State)];
	if (!Cached.IsValid()) Cached = GetPlateTexture(State).LoadSynchronous();
	if (!Cached.IsValid())
	{
		TArray<FColor> Pixels;
		MakePlatePixels(State, Pixels);
		const TArrayView64<const uint8> Bytes(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor));
		if (UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, NAME_None, Bytes))
		{
			Texture->NeverStream = true;
			Texture->UpdateResource();
			Cached = Texture;
		}
	}

	FSlateBrush Brush;
	Brush.SetResourceObject(Cached.Get());
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.Margin = FMargin(0.2f);
	Brush.ImageSize = FVector2D(Size);
	return Brush;
}

namespace
{
FSlateSound MakeSound(ESound Sound)
{
	const auto* Assets = GetDefault<UFactoryUIAssetSettings>();
	const TSoftObjectPtr<USoundBase>& Source = Sound == ESound::Hover ? Assets->HoverSound
		: Sound == ESound::Confirm ? Assets->ConfirmSound
		: Sound == ESound::Cancel ? Assets->CancelSound : Assets->ClickSound;
	FSlateSound Result;
	if (USoundBase* Loaded = Source.LoadSynchronous()) Result.SetResourceObject(Loaded);
	return Result;
}
}

void PlayUISound(ESound Sound)
{
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().PlaySound(MakeSound(Sound));
}

void ApplyButtonSounds(UButton* Button, TOptional<ESound> Press)
{
	if (!Button) return;
	FButtonStyle Style = Button->GetStyle();
	Style.SetHoveredSound(MakeSound(ESound::Hover));
	Style.SetPressedSound(Press.IsSet() ? MakeSound(Press.GetValue()) : FSlateSound());
	Button->SetStyle(Style);
}
}
