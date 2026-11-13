#pragma once

#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Containers/Ticker.h"
#include "AudioSettingsWidget.generated.h"

enum class EGameVolume : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAudioSettingsClosed);

UCLASS(Blueprintable)
class THELASTMESSAGE_API UAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static void BuildDefaultLayout(UWidgetTree* Tree);
	static void BuildVisualLayout(UWidgetTree* Tree, class UVerticalBox* Column);
	static UAudioSettingsWidget* CreateFor(APlayerController* Player);
	void FocusFirstControl();
	void SetPauseInputAction(class UInputAction* Action) { PauseInputAction = Action; }
	FAudioSettingsClosed OnClosed;

	UAudioSettingsWidget(const FObjectInitializer& ObjectInitializer);

protected:
	// 컨트롤의 모양은 각 위젯의 디자이너 스타일을 그대로 쓴다. 여기에는 코드가 새로 만드는 수치 표시와 동작 값만 둔다.

	/** 드롭다운 글자 위치 보정. F_TLM_UI 글자가 줄 높이보다 아래로 그려져서 라벨처럼 위로 올린다. */
	UPROPERTY(EditAnywhere, Category = "Settings Style|Dropdown") FVector2D DropdownTextOffset = FVector2D(0, -5);

	/** 밝기·흔들림·외곽선 굵기 슬라이더의 한 칸 */
	UPROPERTY(EditAnywhere, Category = "Settings Style|Slider", meta = (ClampMin = 0.001)) float VisualSliderStep = 0.05f;

	/** 슬라이더 옆 수치(50%, 2.0×) 스타일. 디자이너에 BrightnessValue / ShakeValue / OutlineWidthValue /
	 *  OutlineColorSwatch 이름의 TextBlock을 직접 두면 그 위젯의 설정이 우선합니다. */
	UPROPERTY(EditAnywhere, Category = "Settings Style|Readout", meta = (ClampMin = 1)) int32 ReadoutFontSize = 17;
	UPROPERTY(EditAnywhere, Category = "Settings Style|Readout") FLinearColor ReadoutColor;
	UPROPERTY(EditAnywhere, Category = "Settings Style|Readout", meta = (ClampMin = 0)) float ReadoutMinWidth = 58.f;
	UPROPERTY(EditAnywhere, Category = "Settings Style|Readout") FMargin ReadoutPadding = FMargin(8, 0, 0, 0);
	UPROPERTY(EditAnywhere, Category = "Settings Style|Readout") FVector2D ReadoutOffset = FVector2D(0, -4);

	/** 화면 모드 적용 후 "유지하기"를 누르지 않으면 되돌리기까지의 시간(초) */
	UPROPERTY(EditAnywhere, Category = "Settings|Screen Mode", meta = (ClampMin = 3, ClampMax = 60)) int32 ScreenConfirmSeconds = 15;
	UPROPERTY(EditAnywhere, Category = "Settings|Screen Mode") FText ApplyScreenLabel;
	/** {0} 자리에 남은 초가 들어갑니다. */
	UPROPERTY(EditAnywhere, Category = "Settings|Screen Mode") FText ConfirmCountdownLabel;

	/** 밝기 슬라이더의 툴팁이 디자이너에서 비어 있을 때 쓰는 기본 문구 */
	UPROPERTY(EditAnywhere, Category = "Settings|Text") FText BrightnessHelpText;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UPROPERTY(Transient) TObjectPtr<class UInputAction> PauseInputAction;
	UFUNCTION() void Close();
	UFUNCTION() void SetMasterVolume(float Value);
	UFUNCTION() void SetEffectsVolume(float Value);
	UFUNCTION() void SetInterfaceVolume(float Value);
	UFUNCTION() void SetMusicVolume(float Value);
	UFUNCTION() void SetAmbienceVolume(float Value);
	void ChangeVolume(EGameVolume Channel, float Value);
	void RefreshVolumeValues();
	void BuildVisualControls();
	UFUNCTION() UWidget* MakeDropdownOption(FString Option);
	void UpdateVisuals();
	void RefreshVisualReadouts();
	void RefreshApplyButton();
	UFUNCTION() void OnResolutionChanged(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetBrightness(float Value);
	UFUNCTION() void SetOutlineWidth(float Value);
	UFUNCTION() void SetShake(float Value);
	UFUNCTION() void SetWindowMode(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetFrameLimit(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetOutlineColor(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetVSync(bool bValue);
	UFUNCTION() void SetMotionBlur(bool bValue);
	UFUNCTION() void SetBloom(bool bValue);
	UFUNCTION() void ApplyScreenMode();
	bool RevertScreenMode(float DeltaTime);
	FTSTicker::FDelegateHandle ScreenConfirmation;
	double ScreenConfirmDeadline = 0.0;
	int32 PreviousWindowMode = 0;
	FIntPoint PreviousResolution = FIntPoint::ZeroValue;
	int32 DisplayedSeconds = 0;
	TArray<FIntPoint> Resolutions;
};
