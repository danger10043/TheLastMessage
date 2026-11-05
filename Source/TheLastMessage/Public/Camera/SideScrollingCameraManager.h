// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "SideScrollingCameraManager.generated.h"

class AAreaMapRegion;
class UCurveFloat;
class AAreaMapRegion;

/** 카메라를 제한하는 방(리전) 하나의 정보. 범위는 주기적으로 갱신해서 저장해 둔다. */
struct FCameraConfineEntry
{
	TWeakObjectPtr<AAreaMapRegion> Region;
	FBox2D Bounds = FBox2D(ForceInit);
};

/** 한 축(X 또는 Z)의 카메라 제한 상태. 방이 바뀌어도 한계값과 적용 정도를 서서히 바꾼다. */
struct FCameraConfineAxis
{
	double Lo = 0.0;
	double Hi = 0.0;
	double Soft = 0.0;
	float Alpha = 0.f;
	bool bHasLimits = false;
};

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

	/** 방이 바뀔 때 카메라 제한이 켜지거나 꺼지는 데 걸리는 시간(초) */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Confine", meta=(ClampMin="0.0", Units="s"))
	float ConfineBlendTime = 0.6f;

	/** 방이 바뀔 때 제한 범위가 새 방의 범위로 따라가는 속도 */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Confine", meta=(ClampMin="0.1"))
	float ConfineLimitSpeed = 2.0f;

	/** 방 경계 앞뒤에서 제한이 왔다 갔다 하지 않도록 두는 여유(cm) */
	UPROPERTY(EditAnywhere, Category="Side Scrolling Camera|Confine", meta=(ClampMin="0", Units="cm"))
	float ConfineSwitchMargin = 100.0f;

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

	// ---- 방(리전)별 카메라 제한 ----
	/** 카메라 제한이 켜진 방과 그 범위. 1초마다 다시 모은다(레벨이 늦게 올라오는 방도 반영). */
	TArray<FCameraConfineEntry> ConfineEntries;
	float ConfineRefreshTimer = 0.0f;
	TWeakObjectPtr<AAreaMapRegion> ActiveConfineRegion;
	FCameraConfineAxis ConfineX;
	FCameraConfineAxis ConfineZ;

	void RefreshConfineEntries();
	void SelectConfineRegion(const FVector2D& PlayerPoint);
	void ApplyConfineAxis(FCameraConfineAxis& Axis, bool bEnabled, double TargetLo, double TargetHi, double Soft,
		double& InOutValue, float DeltaTime, bool bSnap);

	/** 플레이어가 있는 방의 제한을 카메라 목표 위치에 적용한다. */
	void ApplyRegionConfine(float DeltaTime, const FVector2D& PlayerPoint, double HalfWidth, double HalfHeight, FVector& InOutGoal, bool bSnap);

	virtual void UpdateCamera(float DeltaTime) override;
	void UpdateRegionZoom(float DeltaTime);

	TWeakObjectPtr<AAreaMapRegion> ZoomRegion;
	TWeakObjectPtr<APawn> ZoomPawn;
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> ActiveZoomCurve = nullptr;
	float RegionEntryZoom = 0.0f;
	float RegionTargetZoom = 0.0f;
	float RegionZoomDuration = 0.0f;
	float RegionZoomTime = 0.0f;
	bool bInsideZoomRegion = false;
};
