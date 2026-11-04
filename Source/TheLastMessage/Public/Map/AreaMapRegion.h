#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AreaMapRegion.generated.h"

class UBoxComponent;
class UTexture2D;

/** 방의 탐사 식별자와 지도 영역 */
UCLASS()
class THELASTMESSAGE_API AAreaMapRegion : public AActor
{
	GENERATED_BODY()
public:
	AAreaMapRegion();
	/** 배치가 바뀌어도 탐사 기록을 유지하는 방 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area Map", meta=(ToolTip="배치가 바뀌어도 탐사 기록을 유지하는 방 식별자"))
	FName RegionId;
	/** 지도에서 표시할 구역 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area Map", meta=(ToolTip="지도에서 표시할 구역 이름"))
	FText RegionName;
	/** 투명 배경의 X/Z 지도 실루엣. 이미지 전체가 방의 지도 범위에 맞춰진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area Map")
	TObjectPtr<UTexture2D> MapSilhouette;
	/** 방의 기본 탐사 범위 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Area Map", meta=(ToolTip="방의 기본 탐사 범위"))
	TObjectPtr<UBoxComponent> RegionBounds;
	/** 이동한 지형의 위치를 지도 영역에 반영할 기준 액터 */
	UPROPERTY(EditInstanceOnly, Category="Area Map", meta=(ToolTip="이동한 지형의 위치를 지도 영역에 반영할 기준 액터"))
	TArray<TObjectPtr<AActor>> LayoutActors;
	/** 기준 지형이 처음 배치됐을 때의 범위 */
	UPROPERTY(VisibleInstanceOnly, Category="Area Map", meta=(ToolTip="기준 지형이 처음 배치됐을 때의 범위"))
	FBox AuthoredLayoutBounds = FBox(ForceInit);
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Area Map") void CaptureLayoutBounds();
	FBox2D GetMapBounds() const;
};
