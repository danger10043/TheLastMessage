#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AreaMapRegion.generated.h"

class UBoxComponent;
class UTexture2D;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCurveFloat;

/** 방의 탐사 식별자와 지도 영역 */
UCLASS()
class THELASTMESSAGE_API AAreaMapRegion : public AActor
{
	GENERATED_BODY()
public:
	AAreaMapRegion();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	/** 배치가 바뀌어도 탐사 기록을 유지하는 방 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map", meta = (ToolTip = "배치가 바뀌어도 탐사 기록을 유지하는 방 식별자"))
	FName RegionId;
	/** 지도에서 표시할 구역 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map", meta = (ToolTip = "지도에서 표시할 구역 이름"))
	FText RegionName;
	/** 투명 배경의 X/Z 지도 실루엣. 이미지 전체가 방의 지도 범위에 맞춰진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map")
	TObjectPtr<UTexture2D> MapSilhouette;
	/** 방의 기본 탐사 범위 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area Map", meta = (ToolTip = "방의 기본 탐사 범위"))
	TObjectPtr<UBoxComponent> RegionBounds;
	/** 이동한 지형의 위치를 지도 영역에 반영할 기준 액터 */
	UPROPERTY(EditInstanceOnly, Category = "Area Map", meta = (ToolTip = "이동한 지형의 위치를 지도 영역에 반영할 기준 액터"))
	TArray<TObjectPtr<AActor>> LayoutActors;
	/** 기준 지형이 처음 배치됐을 때의 범위 */
	UPROPERTY(VisibleInstanceOnly, Category = "Area Map", meta = (ToolTip = "기준 지형이 처음 배치됐을 때의 범위"))
	FBox AuthoredLayoutBounds = FBox(ForceInit);
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Area Map") void CaptureLayoutBounds();
	FBox2D GetMapBounds() const;

	/** 켜면 플레이어가 이 방 밖에 있을 때 방 전체를 어둡게 가린다. 플레이어가 들어오면 서서히 걷힌다. 기본은 꺼져 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Mask")
	bool bUseMask = false;

	/** 가리는 막에 쓸 머티리얼. 어두운 Unlit 머티리얼에 Opacity 스칼라 파라미터가 있어야 한다(0이면 투명, 1이면 완전히 가림). 양면(Two Sided)으로 만든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask"))
	TObjectPtr<UMaterialInterface> MaskMaterial;

	/** 막으로 쓸 평면 메시(가로세로 100). 기본은 엔진의 Plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask"))
	TObjectPtr<UStaticMesh> MaskMesh;

	/** 머티리얼에서 투명도를 조절하는 스칼라 파라미터 이름 */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask"))
	FName MaskOpacityParameter = TEXT("Opacity");

	/** 방 범위보다 사방으로 더 넓게 덮는 크기(cm). 가장자리가 비치면 키운다. */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask", ClampMin = "0"))
	float MaskPadding = 0.f;

	/**
	 * 막 가장자리를 부드럽게 풀어 주는 폭(cm). 막의 가장자리에서 안쪽으로 이만큼에 걸쳐 서서히 투명해진다. 0이면 각진 가장자리가 된다.
	 * 머티리얼에 스칼라 파라미터 MaskSizeX, MaskSizeZ, MaskFeather가 있어야 적용된다(코드가 값을 넣어 준다).
	 */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask", ClampMin = "0"))
	float MaskFeather = 400.f;

	/**
	 * 끄면(기본) 막의 크기가 방 범위(+Mask Padding)와 같고, 번지는 구간이 방 안쪽에 들어간다. 방 가장자리 쪽은 막이 옅어져 조금 비쳐 보인다.
	 * 켜면 번지는 구간만큼 막이 방 바깥으로 커져서 방 범위 안쪽은 완전히 덮이지만, 막이 이웃 방까지 넘어온다.
	 */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask"))
	bool bFeatherOutsideRegion = false;

	/**
	 * 막을 플레이어가 서 있는 평면에서 카메라 쪽으로 옮기는 거리(cm). 0이면 플레이어 평면에 놓여서 그보다 앞(카메라 쪽)의 전경은 가려지지 않는다.
	 * 양수로 올리면 전경까지 가린다. 카메라와의 거리에 맞춰 크기는 자동으로 보정된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask"))
	float MaskOffsetY = 0.f;

	/**
	 * 방 경계에서 이만큼(cm) 떨어진 곳에 플레이어가 오면 미리 막을 걷기 시작한다. 가장자리 번짐(Mask Feather)보다 작게 줘도 번짐 폭만큼은 자동으로 확보된다.
	 * 막이 플레이어보다 카메라 쪽에 있을 때(Mask Offset Y > 0), 경계를 넘는 순간 플레이어가 막에 가려지는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask", ClampMin = "0"))
	float MaskOpenMargin = 150.f;

	/** 플레이어가 방에 들어왔을 때 막이 서서히 걷히는 시간(초). 이 시간 동안 부드럽게(처음과 끝은 천천히) 투명해진다. */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask", ClampMin = "0"))
	float MaskRevealTime = 0.8f;

	/** 플레이어가 방을 나갔을 때 막이 서서히 다시 덮이는 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask", ClampMin = "0"))
	float MaskCoverTime = 1.0f;

	/** 끄면 한 번 들어가 본 방(지도에 탐사 기록이 있는 방)은 나가도 다시 가리지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Area Map|Mask", meta = (EditCondition = "bUseMask"))
	bool bCoverAgainWhenLeft = true;

	// ---- 카메라 제한 ----

	/** 켜면 플레이어가 이 방 안에 있을 때 카메라가 방 범위 밖을 비추지 않도록 제한한다. 기본은 꺼져 있다. 맵 가장자리처럼 바깥이 보이면 안 되는 방에서만 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Camera")
	bool bConfineCamera = false;

	/** 좌우(X) 방향으로 카메라를 제한한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Camera", meta = (EditCondition = "bConfineCamera"))
	bool bConfineCameraX = true;

	/** 상하(Z) 방향으로 카메라를 제한한다. 세로로 이어지는 통로에서는 끄는 것이 답답하지 않다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Camera", meta = (EditCondition = "bConfineCamera"))
	bool bConfineCameraZ = true;

	/** 화면 가장자리가 방 범위 밖으로 이만큼(cm) 나가는 것을 허용한다. 벽 두께나 장식으로 가려지는 만큼 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Camera", meta = (EditCondition = "bConfineCamera", ClampMin = "0"))
	float CameraOverscan = 250.f;

	/** 한계에 이만큼(cm) 가까워지면 카메라가 점점 느려지며 멈춘다. 클수록 부드럽게 멈춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area Map|Camera", meta = (EditCondition = "bConfineCamera", ClampMin = "0"))
	float CameraSoftZone = 300.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Area Map|Zoom", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetZoomValue = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Area Map|Zoom", meta = (ClampMin = "0.0", Units = "s"))
	float ZoomDuration = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Area Map|Zoom")
	TObjectPtr<UCurveFloat> ZoomCurve = nullptr;

private:
	void UpdateMask(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MaskComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaskMaterialInstance;

	/** 막이 덮인 정도(0 = 걷힘, 1 = 가림). 시간에 따라 일정한 속도로 움직이고, 머티리얼에는 부드러운 곡선으로 바꿔 전달한다. */
	float MaskAlpha = 1.f;
	bool bMaskInitialized = false;
};
