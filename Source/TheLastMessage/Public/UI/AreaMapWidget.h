#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AreaMapWidget.generated.h"

class UAreaMapComponent;
class UInputAction;
class UFont;

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
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual void NativeDestruct() override;
private:
	UPROPERTY() TObjectPtr<UFont> MapFont;
	UPROPERTY() TObjectPtr<UFont> MapTitleFont;
	void RestoreInput();
	float GetMapScale(FVector2D Size) const;
	FVector2D ToMap(FVector2D World, FVector2D Size) const;
	/** 화면 좌표 근처(Radius 픽셀 안)의 빠른 이동 지점으로 이동한다. 마우스 클릭과 패드 A(화면 가운데)가 함께 쓴다. */
	void TryFastTravelAt(FVector2D Pointer, FVector2D Size, double Radius);
	/** 지도 가운데로 플레이어 위치를 옮긴다(F / 패드 Y). */
	void CenterOnPlayer(FVector2D Size);
	bool IsUsingGamepad() const;
	/** 패드 아날로그 입력: 왼쪽 스틱(이동), 트리거(확대·축소). 일시정지 중이라 Slate 이벤트로 받아 Tick에서 적용한다. */
	FVector2D GamepadPan = FVector2D::ZeroVector;
	float GamepadZoomIn = 0.f;
	float GamepadZoomOut = 0.f;
	FVector2D LastMapSize = FVector2D::ZeroVector;
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
