#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameVisualSettingsSubsystem.generated.h"

class APlayerBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMeshComponent;

UCLASS()
class UGameVisualSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	void ApplyAppearance(APlayerBase* Player) const;
	void ApplyPreviewAppearance(USkeletalMeshComponent* Mesh, UMaterialInterface* DefaultBodyMaterial) const;
	static UMaterialInstanceDynamic* CreateOrUpdateBodyTint(USkeletalMeshComponent* Mesh, UMaterialInterface* TintMaterial, const FLinearColor& Color);
	void ApplyDisplay() const;
	void Save() const;
	static float GetFlashIntensity(const UWorld* World);
	void ResetAppearance();
	FLinearColor GetOutlineColor(const APlayerBase* Player) const;
	FLinearColor GetPlayerColor() const;
	static constexpr int32 CustomColorIndex = 6;
	static constexpr float DefaultOutlineScale = 1.f;
	static constexpr float DefaultOutlineBrightness = 1.f;
	FLinearColor CustomOutlineColor = FLinearColor::White;
	FLinearColor CustomPlayerColor = FLinearColor::White;
	float Brightness = 0.5f;
	float OutlineScale = DefaultOutlineScale;
	float OutlineBrightness = DefaultOutlineBrightness;
	float ShakeScale = 1.0f;
	int32 OutlineColor = 0;
	int32 PlayerColor = 0;
	bool bMotionBlur = true;
	bool bBloom = true;
	bool bReduceFlashes = false;
	/** 색각 보정: 0 끄기, 1 적색약(1형), 2 녹색약(2형), 3 청색약(3형). 화면 전체(UI 포함)에 Slate 보정 필터를 건다. */
	int32 ColorVision = 0;
	/** 색각 보정 세기 (0~1) */
	float ColorVisionStrength = 1.f;
	static constexpr int32 ColorVisionTypeCount = 4;

private:
	void ApplyOutline(USkeletalMeshComponent* Mesh) const;
};
