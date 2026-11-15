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
	void ApplyAppearance(APlayerBase* Player) const;
	void ApplyPreviewAppearance(USkeletalMeshComponent* Mesh, UMaterialInterface* DefaultBodyMaterial) const;
	static UMaterialInstanceDynamic* CreateOrUpdateBodyTint(USkeletalMeshComponent* Mesh, UMaterialInterface* TintMaterial, const FLinearColor& Color);
	void ApplyDisplay() const;
	void Save() const;
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

private:
	void ApplyOutline(USkeletalMeshComponent* Mesh) const;
};
