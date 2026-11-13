#include "UI/GameVisualSettingsSubsystem.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
constexpr TCHAR Section[] = TEXT("TheLastMessage.Visual");
const FLinearColor OutlineColors[] = {FLinearColor::White, FLinearColor(0.10f, 0.85f, 1.0f),
	FLinearColor(1.0f, 0.56f, 0.08f), FLinearColor(1.0f, 0.28f, 0.63f), FLinearColor(0.30f, 1.0f, 0.35f)};
}

void UGameVisualSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningDedicatedServer()) return;
	GConfig->GetFloat(Section, TEXT("Brightness"), Brightness, GGameUserSettingsIni);
	GConfig->GetFloat(Section, TEXT("OutlineScale"), OutlineScale, GGameUserSettingsIni);
	GConfig->GetFloat(Section, TEXT("ShakeScale"), ShakeScale, GGameUserSettingsIni);
	GConfig->GetInt(Section, TEXT("OutlineColor"), OutlineColor, GGameUserSettingsIni);
	FString CustomHex;
	if (GConfig->GetString(Section, TEXT("CustomOutlineColor"), CustomHex, GGameUserSettingsIni) && CustomHex.Len() == 8)
		CustomOutlineColor = FLinearColor::FromSRGBColor(FColor::FromHex(CustomHex));
	GConfig->GetBool(Section, TEXT("MotionBlur"), bMotionBlur, GGameUserSettingsIni);
	GConfig->GetBool(Section, TEXT("Bloom"), bBloom, GGameUserSettingsIni);
	Brightness = FMath::IsFinite(Brightness) ? FMath::Clamp(Brightness, 0.f, 1.f) : 0.5f;
	OutlineScale = FMath::IsFinite(OutlineScale) ? FMath::Clamp(OutlineScale, 0.f, 2.f) : 1.f;
	ShakeScale = FMath::IsFinite(ShakeScale) ? FMath::Clamp(ShakeScale, 0.f, 1.f) : 1.f;
	OutlineColor = FMath::Clamp(OutlineColor, 0, CustomColorIndex);
	ApplyDisplay();
}

FLinearColor UGameVisualSettingsSubsystem::GetOutlineColor(const APlayerBase* Player) const
{
	if (OutlineColor == CustomColorIndex) return CustomOutlineColor;
	if (OutlineColor > 0) return OutlineColors[OutlineColor - 1];
	const UMaterialInterface* Source = Player && Player->GetMesh() ? Player->GetMesh()->GetOverlayMaterial() : nullptr;
	if (const auto* Dynamic = Cast<UMaterialInstanceDynamic>(Source)) Source = Dynamic->Parent.Get();
	if (!Source)
		Source = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Main/Blueprints/Character/Player/Materials/MI_PlayerOutline.MI_PlayerOutline"));
	FLinearColor Color = FLinearColor::White;
	if (Source) Source->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Outline Color")), Color);
	return Color;
}

void UGameVisualSettingsSubsystem::ApplyDisplay() const
{
	if (GEngine) GEngine->DisplayGamma = FMath::Lerp(1.7f, 2.7f, Brightness);
	if (auto* Blur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
		Blur->Set(bMotionBlur ? 4 : 0, ECVF_SetByGameOverride);
	if (auto* Bloom = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
		Bloom->Set(bBloom ? 5 : 0, ECVF_SetByGameOverride);
}

void UGameVisualSettingsSubsystem::ApplyOutline(APlayerBase* Player) const
{
	if (!Player || !Player->GetMesh()) return;
	auto* Mesh = Player->GetMesh();
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
		Dynamic = UMaterialInstanceDynamic::Create(Source, Mesh);
		Mesh->SetOverlayMaterial(Dynamic);
	}
	Dynamic->SetScalarParameterValue(TEXT("Outline Width"), Width * OutlineScale);
	Dynamic->SetVectorParameterValue(TEXT("Outline Color"), OutlineColor == 0 ? Color : GetOutlineColor(Player));
}

void UGameVisualSettingsSubsystem::Save() const
{
	GConfig->SetFloat(Section, TEXT("Brightness"), Brightness, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("OutlineScale"), OutlineScale, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("ShakeScale"), ShakeScale, GGameUserSettingsIni);
	GConfig->SetInt(Section, TEXT("OutlineColor"), OutlineColor, GGameUserSettingsIni);
	GConfig->SetString(Section, TEXT("CustomOutlineColor"), *CustomOutlineColor.ToFColorSRGB().ToHex(), GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("MotionBlur"), bMotionBlur, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("Bloom"), bBloom, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
