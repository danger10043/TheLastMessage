#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameVisualSettingsSubsystem.generated.h"

class APlayerBase;

UCLASS()
class UGameVisualSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void ApplyOutline(APlayerBase* Player) const;
	void ApplyDisplay() const;
	void Save() const;
	FLinearColor GetOutlineColor(const APlayerBase* Player) const;
	static constexpr int32 CustomColorIndex = 6;
	FLinearColor CustomOutlineColor = FLinearColor::White;
	float Brightness = 0.5f;
	float OutlineScale = 1.0f;
	float ShakeScale = 1.0f;
	int32 OutlineColor = 0;
	bool bMotionBlur = true;
	bool bBloom = true;
};
