// Camera behavior ported from the UE 5.8 Blueprint side-scroller template.
#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "SideScrollingCameraManager.generated.h"

UCLASS()
class THELASTMESSAGE_API ASideScrollingCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
public:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling Camera", meta=(ClampMin="0"))
	float CurrentZoom = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling Camera")
	float CameraZOffset = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling Camera")
	float CameraXMinBounds = -1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling Camera")
	float CameraXMaxBounds = 9000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling Camera", meta=(ClampMin="0"))
	float InterpolationSpeed = 2.0f;
private:
	float CurrentZ = 0.0f;
	bool bSetup = true;
};
