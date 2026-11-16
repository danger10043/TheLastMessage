#pragma once

#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "Containers/Ticker.h"
#include "Framework/Commands/InputChord.h"
#include "AudioSettingsWidget.generated.h"

enum class EGameVolume : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAudioSettingsClosed);

class UAudioSettingsWidget;

/** 언어 버튼 하나의 클릭을 받아 설정 화면에 어느 언어인지 알려 준다. UButton::OnClicked 에는 인자가 없어서 버튼마다 하나씩 둔다. */
UCLASS(Transient)
class THELASTMESSAGE_API ULanguageButtonHandler : public UObject
{
	GENERATED_BODY()

public:
	FString Culture;
	TWeakObjectPtr<UAudioSettingsWidget> Owner;
	UFUNCTION() void HandleClicked();
};

UCLASS(Blueprintable)
class THELASTMESSAGE_API UAudioSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static void BuildDefaultLayout(UWidgetTree* Tree, bool bIncludeDisplay = false);
	static void BuildVisualLayout(UWidgetTree* Tree, class UVerticalBox* Column);
#if WITH_EDITOR
	static bool AuthorSettingsLayout(UWidgetTree* WidgetTree);
	/** WBP_Settings 에 다섯 번째 탭 "언어"를 넣고 탭 다섯 개가 한 줄에 들어가도록 폭을 나눈다. 탭 글자는 넘치면 줄어든다(ScaleBox). */
	static bool AuthorLanguageTab(UWidgetTree* SettingsTree);
	/** 언어 페이지(WBP_LanguageSettings)의 위젯을 만든다. 2열 × 5행 언어 버튼(국기 + 자국어 이름). */
	static bool AuthorLanguagePage(UWidgetTree* PageTree);
#endif
	static UAudioSettingsWidget* CreateFor(APlayerController* Player);
	void FocusFirstControl();
	/** 설정 화면과 하위 탭 페이지 WBP(WBP_GraphicsSettings 등)를 모두 뒤져 이름으로 위젯을 찾는다. */
	UWidget* FindSettingsWidget(FName Name) const;
	/** 렌더링 해상도를 뺀 품질 그룹이 모두 같은 단계면 그 단계, 섞여 있으면 -1. 해상도는 따로 정하므로 품질 단계 판정에서 뺀다. */
	static int32 GetQualityPresetLevel(const class UGameUserSettings& Screen);
	void SetPauseInputAction(class UInputAction* Action) { PauseInputAction = Action; }
	FAudioSettingsClosed OnClosed;

	UAudioSettingsWidget(const FObjectInitializer& ObjectInitializer);

protected:
	// 컨트롤의 모양은 각 위젯의 디자이너 스타일을 그대로 쓴다. 여기에는 코드가 새로 만드는 수치 표시와 동작 값만 둔다.

	/** 드롭다운 글자 위치 보정. F_TLM_UI 글자가 줄 높이보다 아래로 그려져서 라벨처럼 위로 올린다. */
	UPROPERTY(EditAnywhere, Category = "Settings Style|Dropdown") FVector2D DropdownTextOffset = FVector2D(0, -5);

	/** 카메라와 플레이어 사이의 거리에 대한 가로 구도 보정 비율. 양수는 화면에서 플레이어를 왼쪽으로 이동시킨다. */
	UPROPERTY(EditAnywhere, Category = "Settings Style|Appearance Preview", meta = (ClampMin = "-0.5", ClampMax = "0.5"))
	float AppearancePreviewHorizontalOffset = 0.14f;

	/** 렌더링 해상도 슬라이더의 범위(%)와 한 칸. 출력 해상도는 그대로 두고 3D 장면을 그리는 해상도만 바꾼다. */
	UPROPERTY(EditAnywhere, Category = "Settings|Graphics", meta = (ClampMin = 25, ClampMax = 100)) float MinRenderScalePercent = 50.f;
	UPROPERTY(EditAnywhere, Category = "Settings|Graphics", meta = (ClampMin = 1, ClampMax = 50)) float RenderScaleStepPercent = 5.f;

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
	/** 다른 설정 화면 안에 페이지로 들어가 있으면 true. 이때 동작은 바깥 설정 화면이 맡고 이 위젯은 화면만 제공한다. */
	bool IsNestedSettingsPage() const;
	void ForEachSettingsTree(TFunctionRef<void(UWidgetTree*)> Visit) const;
	UPROPERTY(Transient) TObjectPtr<class UInputAction> PauseInputAction;
	UFUNCTION() void Close();
	UFUNCTION() void SetMasterVolume(float Value);
	UFUNCTION() void SetEffectsVolume(float Value);
	UFUNCTION() void SetInterfaceVolume(float Value);
	UFUNCTION() void SetMusicVolume(float Value);
	UFUNCTION() void SetAmbienceVolume(float Value);
	void ChangeVolume(EGameVolume Channel, float Value);
	void RefreshVolumeValues();
#if WITH_EDITOR
	static void AuthorKeySettingsPage(UWidgetTree* WidgetTree, class UWidgetSwitcher* Pages);
#endif
	void BindKeySelectors();
	void BindSettingsHelp();
	void RefreshSettingsHelp();
	struct FSettingsHelpEntry
	{
		TWeakObjectPtr<UWidget> Row;
		FText Text;
		int32 PageIndex;
	};
	TArray<FSettingsHelpEntry> SettingsHelpEntries;
	TWeakObjectPtr<class UTextBlock> SettingsHelpText;
	void RefreshKeySelectors();
	void UpdateSettingsTabs(int32 ActiveIndex);
	void BuildVisualControls();
	/** 언어가 바뀌면 문자열로 채운 드롭다운을 새 언어로 다시 채운다(FText로 만든 문구는 엔진이 알아서 바꾼다). */
	void HandleCultureChanged();
	/**
	 * 설정 줄 라벨(이름이 Label 로 끝나고 고정 폭 SizeBox 안에 있는 TextBlock)을 ScaleBox 로 감싸서,
	 * 번역으로 길어진 글자가 라벨 칸을 넘으면 줄여서 보여 준다. 이미 감싼 라벨은 건너뛴다.
	 */
	void FitSettingsLabels();
	FDelegateHandle CultureChangedHandle;
	void ShowSettingsTab(int32 Index);
	void RefreshAppearancePreview();
	bool bSynchronizingAppearanceControls = false;
	void SetAppearanceColor(const FString& Value, ESelectInfo::Type Selection, bool bOutline);
	void OpenAppearancePalette(bool bOutline);
	TWeakObjectPtr<class USkeletalMeshComponent> AppearancePreviewMesh;
	UPROPERTY(Transient) TObjectPtr<class UMaterialInterface> AppearancePreviewBody;
	UFUNCTION() UWidget* MakeDropdownOption(FString Option);
	void UpdateVisuals();
	void RefreshVisualReadouts();
	void RefreshApplyButton();
	UFUNCTION() void OnResolutionChanged(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetBrightness(float Value);
	UFUNCTION() void SetOutlineWidth(float Value);
	UFUNCTION() void SetOutlineBrightness(float Value);
	UFUNCTION() void SetShake(float Value);
	UFUNCTION() void SetWindowMode(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetFrameLimit(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetQualityPreset(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetRenderResolutionScale(float Value);
	UFUNCTION() void SetColorVision(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetColorVisionStrength(float Value);
	UFUNCTION() void SetOutlineColor(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void SetPlayerColor(FString Value, ESelectInfo::Type Selection);
	UFUNCTION() void ShowGeneralSettings();
	UFUNCTION() void ShowAudioSettings();
	UFUNCTION() void ShowAppearanceSettings();
	UFUNCTION() void ShowKeySettings();
	UFUNCTION() void ShowLanguageSettings();
	friend class ULanguageButtonHandler;
	void BindLanguageButtons();
	void RefreshLanguageButtons();
	void SelectLanguage(const FString& Culture);
	struct FLanguageButton
	{
		TWeakObjectPtr<class UButton> Button;
		FName Name;
		FString Culture;
		/** 디자이너의 평소 모습(Disabled 브러시). 현재 언어는 여기에 청록 테두리를 얹는다. */
		FSlateBrush BaseBrush;
	};
	TArray<FLanguageButton> LanguageButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<ULanguageButtonHandler>> LanguageHandlers;
	UFUNCTION() void OnKeySelectionModeChanged();
	UFUNCTION() void OnGameplayKeySelected(FInputChord Chord);
	UFUNCTION() void ResetGameplayKeys();
	TMap<class UInputKeySelector*, int32> KeySelectors;
	TWeakObjectPtr<class UInputKeySelector> FirstKeySelector;
	TWeakObjectPtr<class UInputKeySelector> CapturingKey;
	bool bRefreshingKeys = false;
	UFUNCTION() void ResetAppearanceSettings();
	UFUNCTION() void SetVSync(bool bValue);
	UFUNCTION() void SetBloom(bool bValue);
	UFUNCTION() void SetReduceFlashes(bool bValue);
	UFUNCTION() void ApplyScreenMode();
	bool RevertScreenMode(float DeltaTime);
	FTSTicker::FDelegateHandle ScreenConfirmation;
	double ScreenConfirmDeadline = 0.0;
	int32 PreviousWindowMode = 0;
	FIntPoint PreviousResolution = FIntPoint::ZeroValue;
	int32 DisplayedSeconds = 0;
	TArray<FIntPoint> Resolutions;
};
