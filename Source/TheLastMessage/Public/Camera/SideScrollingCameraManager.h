// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "SideScrollingCameraManager.generated.h"

/**
 *  Simple side scrolling camera with smooth scrolling and horizontal bounds
 */
UCLASS()
class ASideScrollingCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:

	/** Overrides the default camera view target calculation */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

public:

	/** How close we want to stay to the view target */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=0, ClampMax=10000, Units="cm"))
	float CurrentZoom = 2000.0f;

	/** How far above the target do we want the camera to focus */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=0, ClampMax=10000, Units="cm"))
	float CameraZOffset = 100.0f;

	/** Position from the top of the screen, mirrored when gravity reverses. */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Framing", meta=(ClampMin="0.5", ClampMax="0.8"))
	float PlayerScreenHeight = 0.75f;

	/** Keep this fraction of the view beyond the supporting floor when no lower floor is found. */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Framing", meta=(ClampMin="0.1", ClampMax="0.4"))
	float GroundBackgroundFraction = 0.23f;

	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Framing", meta=(ClampMin="0.0", ClampMax="0.25"))
	float VerticalDeadZoneFraction = 0.10f;

	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Framing", meta=(ClampMin="0.1"))
	float VerticalFollowSpeed = 3.0f;

	/** Minimum camera scrolling bounds in world space */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=-100000, ClampMax=100000, Units="cm"))
	float CameraXMinBounds = -400.0f;

	/** Maximum camera scrolling bounds in world space */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera", meta=(ClampMin=-100000, ClampMax=100000, Units="cm"))
	float CameraXMaxBounds = 30000.0f;

protected:
	// Follow the stable camera position, never the previous frame's shake offset.
	FVector UnmodifiedCameraLocation = FVector::ZeroVector;
	void FinalizeViewTarget(FTViewTarget& OutVT, float DeltaTime);

	/** Last cached camera vertical location. The camera only adjusts its height if necessary. */
	float CurrentZ = 0.0f;

	/** First-time update camera setup flag */
	bool bSetup = true;
	TWeakObjectPtr<APawn> FollowedPawn;
	FVector PreviousTargetLocation = FVector::ZeroVector;
	float SmoothedFramingOffset = 0.0f;
	float TerrainProbeTimer = 0.0f;
	bool bHasLowerTerrain = true;
};
