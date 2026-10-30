#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AreaMapWidget.generated.h"

class UAreaMapComponent;
class UInputAction;

UCLASS(Blueprintable)
class THELASTMESSAGE_API UAreaMapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	bool OpenMap(UAreaMapComponent* Source, const UInputAction* Action);
	UFUNCTION(BlueprintCallable, Category="Area Map") void CloseMap();
	void SetMapSource(UAreaMapComponent* Source) { MapSource = Source; }
protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual void NativeDestruct() override;
private:
	void RestoreInput();
	float GetMapScale(FVector2D Size) const;
	FVector2D ToMap(FVector2D World, FVector2D Size) const;
	UPROPERTY(Transient) TObjectPtr<UAreaMapComponent> MapSource;
	TWeakObjectPtr<APlayerController> InputOwner;
	TWeakObjectPtr<APawn> ObservedPawn;
	TArray<FKey> ToggleKeys;
	FText TravelStatus;
	FVector2D PressPosition = FVector2D::ZeroVector;
	bool bTravelClick = false;
	FVector2D Pan = FVector2D::ZeroVector;
	float Zoom = 1;
	bool bDragging = false;
	bool bCapturedInput = false;
	bool bPausedByMap = false;
	bool bPreviousCursor = false;
};
