#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "CombatCameraModifier.generated.h"

struct FCombatFeedbackSettings;

/** Real-time camera impulse keeps its punch while world time is almost stopped. */
UCLASS()
class THELASTMESSAGE_API UCombatCameraModifier : public UCameraModifier
{
	GENERATED_BODY()
public:
	void PlayImpulse(const FCombatFeedbackSettings& Settings, float Strength);
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
private:
	double StartRealTime = -1;
	float Duration = 0;
	float Distance = 0;
	float Rotation = 0;
};
