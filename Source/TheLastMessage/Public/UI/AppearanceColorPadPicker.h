#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AppearanceColorPadPicker.generated.h"

class UBorder;
class USlider;
class UTextBlock;
class UButton;

DECLARE_DELEGATE_OneParam(FOnPadColorChanged, FLinearColor);
DECLARE_DELEGATE_TwoParams(FOnPadColorClosed, bool /*bAccepted*/, FLinearColor);

/**
 * 게임패드로 쓰는 사용자 지정 색 고르기 창. 엔진 색상표(SColorPicker)는 마우스 전용이라 패드 모드에서는 이 창을 연다.
 * 색상·채도·밝기 슬라이더 3개: 십자키 ↑↓로 항목, ←→로 조절, A 확인, B 취소. WBP 없이 코드로 화면을 만든다.
 */
UCLASS()
class THELASTMESSAGE_API UAppearanceColorPadPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Title, FLinearColor InitialColor);
	/** 슬라이더를 움직일 때마다(미리보기 적용용). */
	FOnPadColorChanged OnColorChanged;
	/** 확인(true) 또는 취소(false)로 닫힐 때 한 번. */
	FOnPadColorClosed OnClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	void BuildTree();
	USlider* AddSliderRow(class UVerticalBox* Box, const FText& Caption, float StepSize);
	FLinearColor GetColor() const;
	void RefreshPreview();
	void Finish(bool bAccepted);
	UFUNCTION() void HandleSliderChanged(float Value);
	UFUNCTION() void Confirm();
	UFUNCTION() void Cancel();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UBorder> Swatch;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HexText;
	UPROPERTY(Transient) TObjectPtr<USlider> HueSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> SaturationSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> ValueSlider;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
	/** 슬라이더와 같은 순서의 줄 라벨. 포커스가 있는 줄을 청록으로 칠한다. */
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RowLabels;
	bool bFinished = false;
};
