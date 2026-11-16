#include "UI/GameVisualSettingsSubsystem.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
constexpr TCHAR VisualSettingsSection[] = TEXT("TheLastMessage.Visual");
const FLinearColor AppearanceColors[] = {FLinearColor::White, FLinearColor(0.10f, 0.85f, 1.0f),
	FLinearColor(1.0f, 0.56f, 0.08f), FLinearColor(1.0f, 0.28f, 0.63f), FLinearColor(0.30f, 1.0f, 0.35f)};

void ReadCustomColor(const TCHAR* Key, FLinearColor& Color)
{
	FString Hex;
	if (GConfig->GetString(VisualSettingsSection, Key, Hex, GGameUserSettingsIni) && Hex.Len() == 8)
		Color = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
}

UMaterialInterface* LoadPlayerTintMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Test_Jihong/PlayerAppearance/MI_Robbie_PlayerColor.MI_Robbie_PlayerColor"));
}
}

void UGameVisualSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningDedicatedServer()) return;
	GConfig->GetFloat(VisualSettingsSection, TEXT("Brightness"), Brightness, GGameUserSettingsIni);
	GConfig->GetFloat(VisualSettingsSection, TEXT("OutlineScale"), OutlineScale, GGameUserSettingsIni);
	GConfig->GetFloat(VisualSettingsSection, TEXT("OutlineBrightness"), OutlineBrightness, GGameUserSettingsIni);
	GConfig->GetFloat(VisualSettingsSection, TEXT("ShakeScale"), ShakeScale, GGameUserSettingsIni);
	GConfig->GetInt(VisualSettingsSection, TEXT("OutlineColor"), OutlineColor, GGameUserSettingsIni);
	GConfig->GetInt(VisualSettingsSection, TEXT("PlayerColor"), PlayerColor, GGameUserSettingsIni);
	ReadCustomColor(TEXT("CustomOutlineColor"), CustomOutlineColor);
	ReadCustomColor(TEXT("CustomPlayerColor"), CustomPlayerColor);
	GConfig->GetBool(VisualSettingsSection, TEXT("MotionBlur"), bMotionBlur, GGameUserSettingsIni);
	GConfig->GetBool(VisualSettingsSection, TEXT("Bloom"), bBloom, GGameUserSettingsIni);
	GConfig->GetBool(VisualSettingsSection, TEXT("ReduceFlashes"), bReduceFlashes, GGameUserSettingsIni);
	Brightness = FMath::IsFinite(Brightness) ? FMath::Clamp(Brightness, 0.f, 1.f) : 0.5f;
	OutlineScale = FMath::IsFinite(OutlineScale) ? FMath::Clamp(OutlineScale, 0.f, 2.f) : DefaultOutlineScale;
	OutlineBrightness = FMath::IsFinite(OutlineBrightness) ? FMath::Clamp(OutlineBrightness, 0.f, 2.f) : DefaultOutlineBrightness;
	ShakeScale = FMath::IsFinite(ShakeScale) ? FMath::Clamp(ShakeScale, 0.f, 1.f) : 1.f;
	OutlineColor = FMath::Clamp(OutlineColor, 0, CustomColorIndex);
	PlayerColor = FMath::Clamp(PlayerColor, 0, CustomColorIndex);
	ApplyDisplay();
}

FLinearColor UGameVisualSettingsSubsystem::GetOutlineColor(const APlayerBase* Player) const
{
	if (OutlineColor == CustomColorIndex) return CustomOutlineColor;
	if (OutlineColor > 0) return AppearanceColors[OutlineColor - 1];
	const UMaterialInterface* Source = Player && Player->GetMesh() ? Player->GetMesh()->GetOverlayMaterial() : nullptr;
	if (const auto* Dynamic = Cast<UMaterialInstanceDynamic>(Source)) Source = Dynamic->Parent.Get();
	if (!Source)
		Source = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Main/Blueprints/Character/Player/Materials/MI_PlayerOutline.MI_PlayerOutline"));
	FLinearColor Color = FLinearColor::White;
	if (Source) Source->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Outline Color")), Color);
	return Color;
}

FLinearColor UGameVisualSettingsSubsystem::GetPlayerColor() const
{
	if (PlayerColor == CustomColorIndex) return CustomPlayerColor;
	if (PlayerColor > 0) return AppearanceColors[PlayerColor - 1];
	return FLinearColor(0.5f, 0.5f, 0.5f);
}

void UGameVisualSettingsSubsystem::ApplyDisplay() const
{
	if (GEngine) GEngine->DisplayGamma = FMath::Lerp(1.7f, 2.7f, Brightness);
	if (auto* Blur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
		Blur->Set(bMotionBlur ? 4 : 0, ECVF_SetByGameOverride);
	if (auto* Bloom = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
		Bloom->Set(bBloom ? 5 : 0, ECVF_SetByGameOverride);
}

void UGameVisualSettingsSubsystem::ApplyOutline(USkeletalMeshComponent* Mesh) const
{
	if (!Mesh) return;
	auto* Overlay = Mesh->GetOverlayMaterial();
	if (!Overlay) return;
	auto* Dynamic = Cast<UMaterialInstanceDynamic>(Overlay);
	auto* Source = Dynamic ? Dynamic->Parent.Get() : Overlay;
	float Width = 0.f;
	FLinearColor Color;
	if (!Source || !Source->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Outline Width")), Width)
		|| !Source->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Outline Color")), Color)) return;
	if (!Dynamic)
	{
		if (OutlineScale == 1.f && OutlineBrightness == 1.f && OutlineColor == 0) return;
		Dynamic = UMaterialInstanceDynamic::Create(Source, Mesh);
		if (!Dynamic) return;
		Mesh->SetOverlayMaterial(Dynamic);
	}
	Dynamic->SetScalarParameterValue(TEXT("Outline Width"), Width * OutlineScale);
	if (OutlineColor != 0) Color = GetOutlineColor(nullptr);
	const FLinearColor Emission(Color.R * OutlineBrightness, Color.G * OutlineBrightness, Color.B * OutlineBrightness, Color.A);
	Dynamic->SetVectorParameterValue(TEXT("Outline Color"), Emission);
}

UMaterialInstanceDynamic* UGameVisualSettingsSubsystem::CreateOrUpdateBodyTint(USkeletalMeshComponent* Mesh,
	UMaterialInterface* TintMaterial, const FLinearColor& Color)
{
	auto* Dynamic = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!Dynamic || Dynamic->Parent != TintMaterial)
		Dynamic = UMaterialInstanceDynamic::Create(TintMaterial, Mesh);
	if (!Dynamic) return nullptr;
	Dynamic->SetVectorParameterValue(TEXT("Color_1"), Color);
	Dynamic->SetVectorParameterValue(TEXT("Color_2"), Color);
	Dynamic->SetScalarParameterValue(TEXT("Color_1 strength"), 1.f);
	Dynamic->SetScalarParameterValue(TEXT("Color_2 strength"), 0.7f);
	return Dynamic;
}

void UGameVisualSettingsSubsystem::ApplyPreviewAppearance(USkeletalMeshComponent* Mesh, UMaterialInterface* DefaultBodyMaterial) const
{
	if (!Mesh) return;
	ApplyOutline(Mesh);
	UMaterialInterface* Body = DefaultBodyMaterial;
	if (PlayerColor != 0)
	{
		if (auto* TintMaterial = LoadPlayerTintMaterial())
		{
			if (auto* Tint = CreateOrUpdateBodyTint(Mesh, TintMaterial, GetPlayerColor())) Body = Tint;
		}
	}
	if (Mesh->GetMaterial(0) != Body) Mesh->SetMaterial(0, Body);
}

void UGameVisualSettingsSubsystem::ApplyAppearance(APlayerBase* Player) const
{
	if (!Player || IsRunningDedicatedServer()) return;
	ApplyOutline(Player->GetMesh());
	if (PlayerColor == 0)
	{
		Player->SetAppearanceTint(nullptr, FLinearColor::White);
		return;
	}
	if (auto* TintMaterial = LoadPlayerTintMaterial()) Player->SetAppearanceTint(TintMaterial, GetPlayerColor());
}

void UGameVisualSettingsSubsystem::ResetAppearance()
{
	OutlineScale = DefaultOutlineScale;
	OutlineBrightness = DefaultOutlineBrightness;
	OutlineColor = 0;
	PlayerColor = 0;
	CustomOutlineColor = FLinearColor::White;
	CustomPlayerColor = FLinearColor::White;
}

float UGameVisualSettingsSubsystem::GetFlashIntensity(const UWorld* World)
{
	const auto* Settings = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(World ? World->GetGameInstance() : nullptr);
	return Settings && Settings->bReduceFlashes ? 0.25f : 1.f;
}

void UGameVisualSettingsSubsystem::Save() const
{
	GConfig->SetFloat(VisualSettingsSection, TEXT("Brightness"), Brightness, GGameUserSettingsIni);
	GConfig->SetFloat(VisualSettingsSection, TEXT("OutlineScale"), OutlineScale, GGameUserSettingsIni);
	GConfig->SetFloat(VisualSettingsSection, TEXT("OutlineBrightness"), OutlineBrightness, GGameUserSettingsIni);
	GConfig->SetFloat(VisualSettingsSection, TEXT("ShakeScale"), ShakeScale, GGameUserSettingsIni);
	GConfig->SetInt(VisualSettingsSection, TEXT("OutlineColor"), OutlineColor, GGameUserSettingsIni);
	GConfig->SetInt(VisualSettingsSection, TEXT("PlayerColor"), PlayerColor, GGameUserSettingsIni);
	GConfig->SetString(VisualSettingsSection, TEXT("CustomOutlineColor"), *CustomOutlineColor.ToFColorSRGB().ToHex(), GGameUserSettingsIni);
	GConfig->SetString(VisualSettingsSection, TEXT("CustomPlayerColor"), *CustomPlayerColor.ToFColorSRGB().ToHex(), GGameUserSettingsIni);
	GConfig->SetBool(VisualSettingsSection, TEXT("MotionBlur"), bMotionBlur, GGameUserSettingsIni);
	GConfig->SetBool(VisualSettingsSection, TEXT("Bloom"), bBloom, GGameUserSettingsIni);
	GConfig->SetBool(VisualSettingsSection, TEXT("ReduceFlashes"), bReduceFlashes, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
