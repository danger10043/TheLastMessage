#include "UI/AudioSettingsWidget.h"
#include "AppearanceColorComboBox.h"
#include "UI/FactoryMenuStyle.h"
#include "UI/FactoryUIStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/Font.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UI/GameAudioSettingsSubsystem.h"
#include "UI/GameKeySettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/InputKeySelector.h"
#include "Components/Slider.h"
#include "VolumeSlider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Viewport.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"


#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "Character/Player/PlayerBase.h"
#include "UI/GameVisualSettingsSubsystem.h"
#include "UI/GameLanguageSettingsSubsystem.h"
#include "Components/ScaleBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Internationalization/Internationalization.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorPicker.h"


namespace
{
// SettingsTabPages의 페이지 순서. 탭 버튼도 같은 순서로 놓인다.
enum ESettingsPage : int32
{
	GraphicsPage = 0,
	AudioPage = 1,
	AppearancePage = 2,
	KeysPage = 3,
	LanguagePage = 4
};

struct FSettingsTab
{
	const TCHAR* Button;
	const TCHAR* Text;
	int32 Page;
};

// 그래픽 탭은 예전 "화면 및 소리" 탭의 위젯 이름(GeneralSettingsTab)을 그대로 쓴다.
const FSettingsTab SettingsTabs[] = {
	{TEXT("GeneralSettingsTab"), TEXT("GeneralSettingsTabText"), GraphicsPage},
	{TEXT("AudioSettingsTab"), TEXT("AudioSettingsTabText"), AudioPage},
	{TEXT("AppearanceSettingsTab"), TEXT("AppearanceSettingsTabText"), AppearancePage},
	{TEXT("KeySettingsTab"), TEXT("KeySettingsTabText"), KeysPage},
	{TEXT("LanguageSettingsTab"), TEXT("LanguageSettingsTabText"), LanguagePage},
};

FText GetLanguageHelpText() { return NSLOCTEXT("SettingsHelp", "Language", "게임에 표시되는 언어를 바꿉니다."); }

// 드롭다운 항목은 번역되도록 모두 FText(NSLOCTEXT)로 둔다. 전역 static FText는 언어 데이터가 준비되기 전에 만들어질 수 있어서
// 쓸 때마다 함수로 만든다. UComboBoxString은 문자열만 받으므로 채울 때 현재 언어 문자열로 바꾸고,
// 언어가 바뀌면 BuildVisualControls로 다시 채운다. 고른 값은 항목 순서(인덱스)로 판단한다.

// 그래픽 품질 드롭다운 항목. 순서가 Scalability 단계(0 낮음 ~ 3 최상)와 같다.
constexpr int32 QualityPresetCount = 4;
TArray<FText> GetQualityPresetOptions()
{
	return {NSLOCTEXT("SettingsOptions", "QualityLow", "낮음"), NSLOCTEXT("SettingsOptions", "QualityMedium", "중간"),
		NSLOCTEXT("SettingsOptions", "QualityHigh", "높음"), NSLOCTEXT("SettingsOptions", "QualityEpic", "최상")};
}
FText GetCustomQualityOption() { return NSLOCTEXT("SettingsOptions", "QualityCustom", "사용자 지정"); }

// 색각 보정 드롭다운 항목. 순서가 UGameVisualSettingsSubsystem::ColorVision 값과 같다.
TArray<FText> GetColorVisionOptions()
{
	return {NSLOCTEXT("SettingsOptions", "ColorVisionOff", "끄기"), NSLOCTEXT("SettingsOptions", "Protanopia", "적색약 (1형)"),
		NSLOCTEXT("SettingsOptions", "Deuteranopia", "녹색약 (2형)"), NSLOCTEXT("SettingsOptions", "Tritanopia", "청색약 (3형)")};
}

// 화면 모드 드롭다운 항목. 순서가 EWindowMode(0 전체 화면, 1 테두리 없는 창, 2 창 모드)와 같다.
TArray<FText> GetWindowModeOptions()
{
	return {NSLOCTEXT("SettingsOptions", "Fullscreen", "전체 화면"), NSLOCTEXT("SettingsOptions", "WindowedFullscreen", "테두리 없는 창"),
		NSLOCTEXT("SettingsOptions", "Windowed", "창 모드")};
}

// 최대 프레임 드롭다운. 첫 항목(0)은 제한 없음.
constexpr int32 FrameLimitValues[] = {0, 30, 60, 120};
TArray<FText> GetFrameLimitOptions()
{
	TArray<FText> Options = {NSLOCTEXT("SettingsOptions", "FrameLimitNone", "제한 없음")};
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(FrameLimitValues); ++Index) Options.Add(FText::AsNumber(FrameLimitValues[Index]));
	return Options;
}

// 외형 색상 드롭다운 항목. 순서가 UGameVisualSettingsSubsystem의 색상 인덱스와 같고 마지막이 사용자 지정이다.
TArray<FText> GetAppearanceColorOptions()
{
	return {NSLOCTEXT("SettingsOptions", "ColorDefault", "기본"), NSLOCTEXT("SettingsOptions", "ColorWhite", "흰색"),
		NSLOCTEXT("SettingsOptions", "ColorCyan", "청록"), NSLOCTEXT("SettingsOptions", "ColorOrange", "주황"),
		NSLOCTEXT("SettingsOptions", "ColorPink", "분홍"), NSLOCTEXT("SettingsOptions", "ColorLime", "연두"),
		NSLOCTEXT("SettingsOptions", "ColorCustom", "사용자 지정")};
}

TArray<FString> ToOptionStrings(const TArray<FText>& Options)
{
	TArray<FString> Result;
	for (const FText& Option : Options) Result.Add(Option.ToString());
	return Result;
}

const FName DisplayHelpControls[] = {
	TEXT("BrightnessSetting"), TEXT("WindowModeSetting"), TEXT("ResolutionSetting"), TEXT("VSyncSetting"),
	TEXT("FrameLimitSetting"), TEXT("ShakeSetting"), TEXT("BloomSetting"), TEXT("ReduceFlashesSetting"), TEXT("ApplyScreenButton"),
	TEXT("QualityPresetSetting"), TEXT("RenderScaleSetting"), TEXT("ColorVisionSetting"), TEXT("ColorVisionStrengthSetting")
};

/** 현재 렌더링 해상도 비율(%). 엔진이 값을 정하지 않은 상태(0 이하)면 100%로 본다. */
float GetRenderScalePercent(const UGameUserSettings& Screen)
{
	float Normalized = 1.f, Percent = 100.f, MinPercent = 0.f, MaxPercent = 100.f;
	// 읽기 전용 함수지만 엔진 버전에 따라 const가 아니어서 const_cast로 부른다.
	const_cast<UGameUserSettings&>(Screen).GetResolutionScaleInformationEx(Normalized, Percent, MinPercent, MaxPercent);
	return Percent > 0.f ? FMath::Min(Percent, 100.f) : 100.f;
}
const FName AppearanceHelpControls[] = {
	TEXT("OutlineColorSetting"), TEXT("OutlineWidthSetting"), TEXT("OutlineBrightnessSetting"),
	TEXT("PlayerColorSetting"), TEXT("ResetAppearanceButton"), TEXT("AppearancePreviewBox")
};

const FColor PreviewBackgroundEdge(23, 28, 34);
const FColor PreviewBackgroundCenter(52, 58, 66);

void AddPreviewBackdrop(ASkeletalMeshActor* Actor, const FVector& ViewLocation, const FRotator& ViewRotation, double Distance)
{
	auto* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	if (!Material) return;

	constexpr int32 TextureSize = 128;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(TextureSize * TextureSize);
	const FLinearColor Edge = FLinearColor::FromSRGBColor(PreviewBackgroundEdge);
	const FLinearColor Center = FLinearColor::FromSRGBColor(PreviewBackgroundCenter);
	for (int32 Y = 0; Y < TextureSize; ++Y)
	{
		for (int32 X = 0; X < TextureSize; ++X)
		{
			const FVector2D Offset = FVector2D(X, Y) * (2.0 / (TextureSize - 1)) - FVector2D(1.0, 1.0);
			const double Brightness = 1.0 - FMath::SmoothStep(0.0, 1.0, Offset.Size());
			Pixels[Y * TextureSize + X] = FMath::Lerp(Edge, Center, Brightness).ToFColorSRGB();
		}
	}
	auto* Texture = UTexture2D::CreateTransient(TextureSize, TextureSize, PF_B8G8R8A8, NAME_None,
		MakeArrayView(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)));
	if (!Texture) return;

	auto* Backdrop = NewObject<UProceduralMeshComponent>(Actor);
	Actor->AddInstanceComponent(Backdrop);
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop->SetCastShadow(false);
	Backdrop->SetReceivesDecals(false);
	auto* Gradient = UMaterialInstanceDynamic::Create(Material, Backdrop);
	Gradient->SetTextureParameterValue(TEXT("SlateUI"), Texture);
	// Supply the same material inputs as a world-space widget, with an opaque, untinted backdrop.
	Gradient->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor::White);
	Gradient->SetVectorParameterValue(TEXT("BackColor"), Edge);
	Gradient->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.0f);
	Backdrop->SetMaterial(0, Gradient);

	const double Depth = 3.0 * Distance;
	const double Extent = Depth * 1.05;
	const FVector PlaneCenter = ViewLocation + ViewRotation.Vector() * Depth;
	const FVector Right = ViewRotation.RotateVector(FVector::RightVector) * Extent;
	const FVector Up = ViewRotation.RotateVector(FVector::UpVector) * Extent;
	Backdrop->CreateMeshSection_LinearColor(0,
		{PlaneCenter - Right + Up, PlaneCenter + Right + Up, PlaneCenter - Right - Up, PlaneCenter + Right - Up},
		{0, 1, 2, 2, 1, 3}, {}, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {}, {}, false);
	Backdrop->RegisterComponent();
}

FSlateRoundedBoxBrush MakeControlBrush(const FLinearColor& Fill, const FLinearColor& Edge, const FVector2D& Size)
{
	auto Brush = FactoryUI::Plate(Fill, Edge, 3.f);
	Brush.ImageSize = Size;
	return Brush;
}

void StyleSettingsSlider(USlider* Slider)
{
	FSliderStyle Style = Slider->GetWidgetStyle();
	Style.NormalBarImage = FactoryUI::Plate(FactoryUI::Panel, FactoryUI::Edge, 2.f);
	Style.HoveredBarImage = FactoryUI::Plate(FactoryUI::Panel, FactoryUI::Cyan, 2.f);
	Style.DisabledBarImage = FactoryUI::Plate(FactoryUI::Panel, FactoryUI::TextMuted, 2.f);
	Style.BarThickness = 6.f;
	const FVector2D ThumbSize(12.f, 18.f);
	Style.NormalThumbImage = MakeControlBrush(FactoryUI::Cyan, FactoryUI::Text, ThumbSize);
	Style.HoveredThumbImage = MakeControlBrush(FactoryUI::Amber, FactoryUI::Text, ThumbSize);
	Style.DisabledThumbImage = MakeControlBrush(FactoryUI::TextMuted, FactoryUI::Edge, ThumbSize);
	Slider->SetWidgetStyle(Style);
	Slider->SetSliderBarColor(FLinearColor::White);
	Slider->SetSliderHandleColor(FLinearColor::White);
}

void StyleSettingsCheckBox(UCheckBox* Check)
{
	FCheckBoxStyle Style = Check->GetWidgetStyle();
	const FVector2D BoxSize(20.f, 20.f);
	Style.UncheckedImage = MakeControlBrush(FactoryUI::Panel, FactoryUI::Edge, BoxSize);
	Style.UncheckedHoveredImage = MakeControlBrush(FactoryUI::Panel, FactoryUI::Cyan, BoxSize);
	Style.UncheckedPressedImage = MakeControlBrush(FactoryUI::Panel, FactoryUI::Amber, BoxSize);
	Style.CheckedImage = MakeControlBrush(FactoryUI::Cyan, FactoryUI::Text, BoxSize);
	Style.CheckedHoveredImage = MakeControlBrush(FactoryUI::Cyan, FactoryUI::Amber, BoxSize);
	Style.CheckedPressedImage = MakeControlBrush(FactoryUI::Amber, FactoryUI::Text, BoxSize);
	Check->SetWidgetStyle(Style);
}

struct FVolumeRow
{
	EGameVolume Channel;
	const TCHAR* Slider;
	const TCHAR* Percent;
	FText Label;
};

const FVolumeRow VolumeRows[] = {
	{EGameVolume::Master, TEXT("MasterVolumeSlider"), TEXT("MasterVolumePercent"), NSLOCTEXT("Pause", "MasterVolume", "전체 볼륨")},
	{EGameVolume::Music, TEXT("MusicVolumeSlider"), TEXT("MusicVolumePercent"), NSLOCTEXT("Pause", "MusicVolume", "배경 음악")},
	{EGameVolume::Effects, TEXT("EffectsVolumeSlider"), TEXT("EffectsVolumePercent"), NSLOCTEXT("Pause", "EffectsVolume", "효과음")},
	{EGameVolume::Ambience, TEXT("AmbienceVolumeSlider"), TEXT("AmbienceVolumePercent"), NSLOCTEXT("Pause", "AmbienceVolume", "환경음")},
	{EGameVolume::Interface, TEXT("InterfaceVolumeSlider"), TEXT("InterfaceVolumePercent"), NSLOCTEXT("Pause", "InterfaceVolume", "UI 소리")},
};

void AddVolumeRows(UWidgetTree* Tree, UVerticalBox* Settings)
{

	UVerticalBox* Controls = Tree->ConstructWidget<UVerticalBox>();
	Settings->AddChildToVerticalBox(Controls)->SetPadding(FMargin(18, 8, 18, 14));

	for (const FVolumeRow& Entry : VolumeRows)
	{
		auto* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetHeightOverride(59);
		Controls->AddChildToVerticalBox(Size);
		auto* Row = Tree->ConstructWidget<UHorizontalBox>();
		Size->AddChild(Row);

		auto AddText = [&](FName Name, const FText& Text, float Width, int32 FontSize, ETextJustify::Type Justification)
		{
			auto* Box = Tree->ConstructWidget<USizeBox>();
			Box->SetWidthOverride(Width);
			Row->AddChildToHorizontalBox(Box)->SetVerticalAlignment(VAlign_Center);
			auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			auto Font = Label->GetFont();
			if (UFont* BodyFont = GetDefault<UFactoryUIAssetSettings>()->BodyFont.LoadSynchronous()) Font.FontObject = BodyFont;
			Font.Size = FontSize;
			Label->SetFont(Font);
			Label->SetText(Text);
			Label->SetJustification(Justification);
			Label->SetColorAndOpacity(FactoryUI::Text);
			Box->AddChild(Label);
		};

		AddText(FName(*(FString(Entry.Slider) + TEXT("Label"))), Entry.Label, 132, 22, ETextJustify::Left);

		auto* Slider = Tree->ConstructWidget<UVolumeSlider>(UVolumeSlider::StaticClass(), Entry.Slider);
		StyleSettingsSlider(Slider);
		Slider->SetIndentHandle(false);
		Slider->SetToolTipText(NSLOCTEXT("Pause", "VolumeMuteHint", "손잡이 클릭으로 음소거 또는 이전 볼륨 복원"));
		Slider->SetValue(1.0f);
		Slider->SetStepSize(0.01f);
		Slider->MouseUsesStep = true;
		Slider->RequiresControllerLock = false;
		Slider->IsFocusable = true;
		auto* SliderSlot = Row->AddChildToHorizontalBox(Slider);
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Fill);
		SliderSlot->SetPadding(FMargin(14, 0, 16, 0));
		AddText(Entry.Percent, FText::AsPercent(1.0f), 68, 19, ETextJustify::Right);

		if (Entry.Channel != EGameVolume::Interface)
		{
			auto* Divider = Tree->ConstructWidget<UImage>();
			FSlateRoundedBoxBrush Brush(FLinearColor(0.14f, 0.23f, 0.28f, 0.72f), 0.0f, FVector2f(1, 1));
			Divider->SetBrush(Brush);
			Divider->SetVisibility(ESlateVisibility::HitTestInvisible);
			Controls->AddChildToVerticalBox(Divider);
		}
	}
}
}

UAudioSettingsWidget::UAudioSettingsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ReadoutColor(FactoryUI::Text)
	, ApplyScreenLabel(NSLOCTEXT("Settings", "ApplyScreen", "화면 모드 적용"))
	, ConfirmCountdownLabel(NSLOCTEXT("Settings", "ConfirmCountdown", "유지하기 ({0}초)"))
	, BrightnessHelpText(NSLOCTEXT("Settings", "BrightnessHelp", "배경의 어두운 곳에서 발판의 윤곽이 희미하게 보이도록 조절하세요."))
{
}

void UAudioSettingsWidget::BuildDefaultLayout(UWidgetTree* Tree, bool bIncludeDisplay)
{
	auto* Pages = FactoryMenuStyle::BuildShell(Tree, true);
	auto* Panel = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AudioSettingsPanel"));
	Pages->AddChild(Panel);
	auto* Heading = FactoryMenuStyle::AddText(Tree, Panel, TEXT("SettingsHeading"), NSLOCTEXT("Pause", "Settings", "설정"), 32);
	auto Font = Heading->GetFont();
	if (UFont* Title = GetDefault<UFactoryUIAssetSettings>()->TitleFont.LoadSynchronous()) Font.FontObject = Title;
	Heading->SetFont(Font);
	if (bIncludeDisplay)
	{
		auto* Columns = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsColumns"));
		Panel->AddChildToVerticalBox(Columns);
		auto* Visual = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VisualSettingsColumn"));
		auto* Audio = Tree->ConstructWidget<UVerticalBox>();
		Columns->AddChildToHorizontalBox(Visual)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Columns->AddChildToHorizontalBox(Audio)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BuildVisualLayout(Tree, Visual);
		AddVolumeRows(Tree, Audio);
	}
	else
	{
		AddVolumeRows(Tree, Panel);
	}
	FactoryMenuStyle::AddButton(Tree, Panel, TEXT("SettingsBackButton"), TEXT("SettingsBackText"), NSLOCTEXT("Pause", "Back", "뒤로"), true);
	if (auto* Hint = Cast<UTextBlock>(Tree->FindWidget(TEXT("NavigationHint"))))
		Hint->SetText(NSLOCTEXT("Pause", "VolumeNavigation", "↑↓ 선택    ←→ 볼륨 조절"));
}

bool UAudioSettingsWidget::IsNestedSettingsPage() const
{
	// 다른 설정 화면 안에 들어간 페이지(WBP_AudioSettings 등)는 화면만 제공하고, 동작은 바깥 설정 화면이 맡는다.
	return GetTypedOuter<UAudioSettingsWidget>() != nullptr;
}

void UAudioSettingsWidget::ForEachSettingsTree(TFunctionRef<void(UWidgetTree*)> Visit) const
{
	if (!WidgetTree) return;
	Visit(WidgetTree);
	// 하위 탭은 각각 별도 WBP(WBP_GraphicsSettings 등)라서 위젯이 그 위젯 트리 안에 있다. 한 단계 아래까지 함께 찾는다.
	TArray<UWidgetTree*> PageTrees;
	WidgetTree->ForEachWidget([&PageTrees](UWidget* Widget)
	{
		if (const auto* Page = Cast<UUserWidget>(Widget); Page && Page->WidgetTree) PageTrees.Add(Page->WidgetTree);
	});
	for (UWidgetTree* Tree : PageTrees) Visit(Tree);
}

UWidget* UAudioSettingsWidget::FindSettingsWidget(FName Name) const
{
	UWidget* Found = nullptr;
	ForEachSettingsTree([&Found, Name](UWidgetTree* Tree)
	{
		if (!Found) Found = Tree->FindWidget(Name);
	});
	return Found;
}

TSharedRef<SWidget> UAudioSettingsWidget::RebuildWidget()
{
	return FactoryMenuStyle::WithMenuFocusStyle(Super::RebuildWidget());
}

UAudioSettingsWidget* UAudioSettingsWidget::CreateFor(APlayerController* Player)
{
	if (!Player) return nullptr;
	UClass* Class = LoadClass<UAudioSettingsWidget>(nullptr, TEXT("/Game/Main/UI/Menu/WBP_Settings.WBP_Settings_C"));
	return Class ? CreateWidget<UAudioSettingsWidget>(Player, Class) : nullptr;
}

void UAudioSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsNestedSettingsPage()) return;
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("SettingsBackButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Close);
		FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Cancel);
	}
	if (auto* Slider = Cast<USlider>(FindSettingsWidget(VolumeRows[0].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetMasterVolume);
	if (auto* Slider = Cast<USlider>(FindSettingsWidget(VolumeRows[1].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetMusicVolume);
	if (auto* Slider = Cast<USlider>(FindSettingsWidget(VolumeRows[2].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetEffectsVolume);
	if (auto* Slider = Cast<USlider>(FindSettingsWidget(VolumeRows[3].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetAmbienceVolume);
	if (auto* Slider = Cast<USlider>(FindSettingsWidget(VolumeRows[4].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetInterfaceVolume);
	for (const FVolumeRow& Entry : VolumeRows)
	{
		auto* Slider = Cast<USlider>(FindSettingsWidget(Entry.Slider));
		if (!Slider) continue;
		StyleSettingsSlider(Slider);
		if (auto* VolumeSlider = Cast<UVolumeSlider>(Slider))
			VolumeSlider->OnToggleMute.BindWeakLambda(this, [this, Channel = Entry.Channel]
			{
				if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance()))
				{
					Audio->ToggleMute(Channel);
					RefreshVolumeValues();
				}
			});
	}

	RefreshVolumeValues();
	BuildVisualControls();
	if (!CultureChangedHandle.IsValid())
		CultureChangedHandle = FInternationalization::Get().OnCultureChanged().AddUObject(this, &ThisClass::HandleCultureChanged);
	BindKeySelectors();
	BindSettingsHelp();
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("GeneralSettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowGeneralSettings);
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("AudioSettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAudioSettings);
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("AppearanceSettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAppearanceSettings);
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("KeySettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowKeySettings);
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("LanguageSettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowLanguageSettings);
	BindLanguageButtons();
	for (const FSettingsTab& Tab : SettingsTabs)
		if (auto* Button = Cast<UButton>(FindSettingsWidget(Tab.Button)))
			FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Confirm);
	if (auto* Button = Cast<UButton>(FindSettingsWidget(TEXT("ResetAppearanceButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetAppearanceSettings);
		FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Confirm);
	}
}

void UAudioSettingsWidget::FocusFirstControl()
{
	ForceLayoutPrepass();
	auto* Pages = Cast<UWidgetSwitcher>(FindSettingsWidget(TEXT("SettingsTabPages")));
	const int32 Page = Pages ? Pages->GetActiveWidgetIndex() : INDEX_NONE;
	UWidget* Target = nullptr;
	if (Page == AppearancePage) Target = FindSettingsWidget(TEXT("OutlineColorSetting"));
	else if (Page == KeysPage) Target = FirstKeySelector.Get();
	else if (Page == AudioPage) Target = FindSettingsWidget(VolumeRows[0].Slider);
	else if (Page == LanguagePage)
	{
		// 현재 언어 버튼에서 시작한다. 방향키로 버튼 사이를 옮기고 Enter로 고른다.
		const FString Current = UGameLanguageSettingsSubsystem::GetCurrentLanguage();
		for (const FLanguageButton& Entry : LanguageButtons)
			if (Entry.Button.IsValid() && (!Target || Entry.Culture == Current))
				Target = Entry.Button.Get();
	}
	else Target = FindSettingsWidget(TEXT("BrightnessSetting"));
	if (!Target) Target = FindSettingsWidget(VolumeRows[0].Slider);
	if (Target) Target->SetKeyboardFocus();
}

void UAudioSettingsWidget::Close()
{
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance())) Audio->SaveVolumes();
	OnClosed.Broadcast();
}

void UAudioSettingsWidget::NativeDestruct()
{
	if (IsNestedSettingsPage())
	{
		Super::NativeDestruct();
		return;
	}
	if (CultureChangedHandle.IsValid())
	{
		FInternationalization::Get().OnCultureChanged().Remove(CultureChangedHandle);
		CultureChangedHandle.Reset();
	}
	if (ScreenConfirmation.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ScreenConfirmation);
		RevertScreenMode(0.f);
	}
	// 렌더링 해상도 슬라이더는 움직이는 동안 저장하지 않으므로 닫을 때 한 번 저장한다.
	// (화면 모드 확인 중이었다면 위에서 되돌리면서 이미 저장했다.)
	else if (auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr; Screen && WidgetTree && FindSettingsWidget(TEXT("RenderScaleSetting")))
		Screen->SaveSettings();
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance())) Audio->SaveVolumes();
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->Save();
	Super::NativeDestruct();
}

FReply UAudioSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (IsNestedSettingsPage()) return Super::NativeOnPreviewKeyDown(Geometry, Event);
	if (const auto* Selector = CapturingKey.Get(); Selector && Selector->GetIsSelectingKey())
		return Super::NativeOnPreviewKeyDown(Geometry, Event);
	const auto* Inputs = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetOwningLocalPlayer());
	if (Event.GetKey() == EKeys::Escape
		|| (Inputs && PauseInputAction && Inputs->QueryKeysMappedToAction(PauseInputAction).Contains(Event.GetKey())))
	{
		if (Event.IsRepeat()) return FReply::Handled();
		FactoryUI::PlayUISound(FactoryUI::ESound::Cancel);
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UAudioSettingsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (IsNestedSettingsPage()) return;
	const auto UpdateFocus = [this] { ForEachSettingsTree([](UWidgetTree* Tree) { FactoryMenuStyle::UpdateFocus(Tree); }); };
	RefreshSettingsHelp();
	if (ScreenConfirmation.IsValid())
	{
		const double Remaining = ScreenConfirmDeadline - FPlatformTime::Seconds();
		// 일시정지 중이거나 화면 모드가 바뀌는 동안 코어 티커가 밀릴 수 있어서, 실제 시간 기준으로 여기서 직접 되돌린다.
		if (Remaining <= 0.0)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(ScreenConfirmation);
			RevertScreenMode(0.f);
			UpdateFocus();
			return;
		}
		const int32 Seconds = FMath::Max(1, FMath::CeilToInt(Remaining));
		if (DisplayedSeconds != Seconds)
		{
			DisplayedSeconds = Seconds;
			if (auto* Text = Cast<UTextBlock>(FindSettingsWidget(TEXT("ApplyScreenText"))))
				Text->SetText(FText::Format(ConfirmCountdownLabel, Seconds));
		}
	}
	UpdateFocus();
}

void UAudioSettingsWidget::RefreshVolumeValues()
{
	const auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance());
	for (const FVolumeRow& Entry : VolumeRows)
	{
		const float Value = Audio ? Audio->GetVolume(Entry.Channel) : 1.0f;
		if (auto* Slider = Cast<USlider>(FindSettingsWidget(Entry.Slider))) Slider->SetValue(Value);
		if (auto* Label = Cast<UTextBlock>(FindSettingsWidget(Entry.Percent))) Label->SetText(FText::AsPercent(Value));
	}
}

void UAudioSettingsWidget::ChangeVolume(EGameVolume Channel, float Value)
{
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance()))
	{
		Audio->SetVolume(Channel, Value);
		RefreshVolumeValues();
	}
}

void UAudioSettingsWidget::SetMasterVolume(float Value) { ChangeVolume(EGameVolume::Master, Value); }
void UAudioSettingsWidget::SetEffectsVolume(float Value) { ChangeVolume(EGameVolume::Effects, Value); }
void UAudioSettingsWidget::SetInterfaceVolume(float Value) { ChangeVolume(EGameVolume::Interface, Value); }
void UAudioSettingsWidget::SetMusicVolume(float Value) { ChangeVolume(EGameVolume::Music, Value); }
void UAudioSettingsWidget::SetAmbienceVolume(float Value) { ChangeVolume(EGameVolume::Ambience, Value); }



namespace
{
UTextBlock* MakeSettingsLabel(UWidgetTree* Tree, const FText& Caption, int32 Size)
{
	auto* Label = Tree->ConstructWidget<UTextBlock>();
	auto Font = Label->GetFont();
	Font.FontObject = GetDefault<UFactoryUIAssetSettings>()->BodyFont.LoadSynchronous();
	Font.Size = Size;
	Label->SetFont(Font);
	Label->SetText(Caption);
	Label->SetColorAndOpacity(FactoryUI::Text);
	return Label;
}

void AddSettingsRow(UWidgetTree* Tree, UVerticalBox* Column, const FText& Caption, UWidget* Control)
{
	auto* Row = Tree->ConstructWidget<UHorizontalBox>();
	auto* Height = Tree->ConstructWidget<USizeBox>();
	Height->SetHeightOverride(32.f);
	Height->AddChild(Row);
	Column->AddChildToVerticalBox(Height)->SetPadding(FMargin(0, 2));
	auto* LabelWidth = Tree->ConstructWidget<USizeBox>();
	LabelWidth->SetWidthOverride(179.f);
	auto* Label = MakeSettingsLabel(Tree, Caption, 19);
	Label->Rename(*(Control->GetName() + TEXT("Label")), Tree);
	// Align the visible glyphs with the control center despite the font's line metrics.
	Label->SetRenderTranslation(FVector2D(0, -5));
	LabelWidth->AddChild(Label);
	Row->AddChildToHorizontalBox(LabelWidth)->SetVerticalAlignment(VAlign_Center);
	auto* Slot = Row->AddChildToHorizontalBox(Control);
	Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Slot->SetVerticalAlignment(VAlign_Center);
}
}

void UAudioSettingsWidget::BuildVisualLayout(UWidgetTree* Tree, UVerticalBox* Column)
{
	if (!Column) return;
	const FText BloomLabel = NSLOCTEXT("Settings", "Bloom", "빛 번짐");
	const auto AddReduceFlashesRow = [&]
	{
		auto* Check = Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("ReduceFlashesSetting"));
		Check->SetToolTipText(NSLOCTEXT("Settings", "ReduceFlashesTip", "피격과 에너지 무기 발사 시 번쩍임을 줄입니다."));
		AddSettingsRow(Tree, Column, NSLOCTEXT("Settings", "ReduceFlashes", "섬광 줄이기"), Check);
	};
	if (Column->GetChildrenCount())
	{
		if (auto* MotionBlur = Tree->FindWidget(TEXT("MotionBlurSetting")))
		{
			UWidget* Row = MotionBlur;
			while (Row && Row->GetParent() != Column) Row = Row->GetParent();
			if (Row) Row->RemoveFromParent();
		}
		if (auto* Label = Cast<UTextBlock>(Tree->FindWidget(TEXT("BloomSettingLabel"))))
			Label->SetText(BloomLabel);
		if (!Tree->FindWidget(TEXT("ReduceFlashesSetting")))
		{
			UWidget* BloomRow = Tree->FindWidget(TEXT("BloomSetting"));
			while (BloomRow && BloomRow->GetParent() != Column) BloomRow = BloomRow->GetParent();
			AddReduceFlashesRow();
			if (BloomRow) Column->ShiftChild(Column->GetChildIndex(BloomRow) + 1, Column->GetChildAt(Column->GetChildrenCount() - 1));
		}
		return;
	}
	auto AddRow = [&](const FText& Caption, UWidget* Control) { AddSettingsRow(Tree, Column, Caption, Control); };

	Column->AddChildToVerticalBox(MakeSettingsLabel(Tree, NSLOCTEXT("Settings", "DisplayHeading", "화면"), 24));
	AddRow(NSLOCTEXT("Settings", "Brightness", "밝기"), Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("BrightnessSetting")));
	AddRow(NSLOCTEXT("Settings", "WindowMode", "화면 모드"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("WindowModeSetting")));
	AddRow(NSLOCTEXT("Settings", "Resolution", "해상도"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ResolutionSetting")));
	AddRow(NSLOCTEXT("Settings", "VSync", "수직 동기화"), Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("VSyncSetting")));
	AddRow(NSLOCTEXT("Settings", "FrameLimit", "최대 프레임"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("FrameLimitSetting")));
	AddRow(NSLOCTEXT("Settings", "Shake", "화면 흔들림"), Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ShakeSetting")));
	AddRow(BloomLabel, Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("BloomSetting")));
	AddReduceFlashesRow();
	AddRow(NSLOCTEXT("Settings", "OutlineColor", "외곽선 색상"), Tree->ConstructWidget<UAppearanceColorComboBox>(UAppearanceColorComboBox::StaticClass(), TEXT("OutlineColorSetting")));
	AddRow(NSLOCTEXT("Settings", "OutlineWidth", "외곽선 굵기"), Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("OutlineWidthSetting")));
	FactoryMenuStyle::AddButton(Tree, Column, TEXT("ApplyScreenButton"), TEXT("ApplyScreenText"),
		NSLOCTEXT("Settings", "ApplyScreen", "화면 모드 적용"), true);
	auto* Apply = CastChecked<UButton>(Tree->FindWidget(TEXT("ApplyScreenButton")));
	if (auto* Box = Cast<USizeBox>(Apply->GetParent())) Box->SetHeightOverride(44);
}

#if WITH_EDITOR
namespace
{
bool PlaceSettingsHelpInCanvas(UWidgetTree* Tree, USizeBox* Space)
{
	auto* Artwork = Tree->FindWidget(TEXT("PausePanelArtwork"));
	auto* Canvas = Artwork ? Cast<UCanvasPanel>(Artwork->GetParent()) : nullptr;
	auto* Design = Cast<USizeBox>(Tree->FindWidget(TEXT("DesignResolution")));
	auto* Page = Tree->FindWidget(TEXT("PausePages"));
	auto* Hint = Tree->FindWidget(TEXT("NavigationHint"));
	auto* ArtworkSlot = Artwork ? Cast<UCanvasPanelSlot>(Artwork->Slot) : nullptr;
	auto* PageSlot = Page ? Cast<UCanvasPanelSlot>(Page->Slot) : nullptr;
	auto* HintSlot = Hint ? Cast<UCanvasPanelSlot>(Hint->Slot) : nullptr;
	if (!Space || !Canvas || !Design || !ArtworkSlot || !PageSlot || !HintSlot
		|| Page->GetParent() != Canvas || Hint->GetParent() != Canvas) return false;
	if (Space->GetParent() == Canvas) return true;

	const FVector2D DesignSize(Design->GetWidthOverride(), Design->GetHeightOverride());
	const auto ReadRect = [&DesignSize](const UCanvasPanelSlot& CanvasSlot)
	{
		const FAnchors Anchors = CanvasSlot.GetAnchors();
		const FVector2D Alignment = CanvasSlot.GetAlignment();
		const FVector2D Size = CanvasSlot.GetSize();
		FVector2D Start = Anchors.Minimum * DesignSize + CanvasSlot.GetPosition() - Alignment * Size;
		FVector2D End = Start + Size;
		if (Anchors.IsStretchedHorizontal())
		{
			Start.X = Anchors.Minimum.X * DesignSize.X + CanvasSlot.GetOffsets().Left;
			End.X = Anchors.Maximum.X * DesignSize.X - CanvasSlot.GetOffsets().Right;
		}
		if (Anchors.IsStretchedVertical())
		{
			Start.Y = Anchors.Minimum.Y * DesignSize.Y + CanvasSlot.GetOffsets().Top;
			End.Y = Anchors.Maximum.Y * DesignSize.Y - CanvasSlot.GetOffsets().Bottom;
		}
		return FBox2D(Start, End);
	};
	const auto WriteRect = [](UCanvasPanelSlot& CanvasSlot, const FBox2D& Rect)
	{
		CanvasSlot.SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot.SetAlignment(FVector2D::ZeroVector);
		CanvasSlot.SetPosition(Rect.Min);
		CanvasSlot.SetSize(Rect.GetSize());
	};

	FBox2D Panel = ReadRect(*ArtworkSlot);
	FBox2D Content = ReadRect(*PageSlot);
	FBox2D Navigation = ReadRect(*HintSlot);
	constexpr double GrowthPerEdge = 64.0;
	constexpr double CanvasMargin = 24.0;
	const double TopGrowth = FMath::Clamp(Panel.Min.Y - CanvasMargin, 0.0, GrowthPerEdge);
	const double BottomGrowth = FMath::Clamp(DesignSize.Y - CanvasMargin - Panel.Max.Y, 0.0, GrowthPerEdge);
	Panel.Min.Y -= TopGrowth;
	Panel.Max.Y += BottomGrowth;
	Content.Min.Y -= TopGrowth;
	Navigation = Navigation.ShiftBy(FVector2D(0.0, BottomGrowth));

	const double HelpBottom = Navigation.Min.Y - 12.0;
	const FBox2D Help(FVector2D(Content.Min.X + 4.0, HelpBottom - Space->GetHeightOverride()),
		FVector2D(Content.Max.X - 4.0, HelpBottom));
	Content.Max.Y = Help.Min.Y - 12.0;
	if (Content.GetSize().Y <= 0.0 || Help.Min.Y < Panel.Min.Y || Help.Max.Y > Panel.Max.Y) return false;
	WriteRect(*ArtworkSlot, Panel);
	WriteRect(*PageSlot, Content);
	WriteRect(*HintSlot, Navigation);

	UWidget* Back = Tree->FindWidget(TEXT("SettingsBackButton"));
	while (Back && Back->GetParent() && Back->GetParent() != Canvas) Back = Back->GetParent();
	if (Back && Back != Page)
		if (auto* BackSlot = Cast<UCanvasPanelSlot>(Back->Slot))
			WriteRect(*BackSlot, ReadRect(*BackSlot).ShiftBy(FVector2D(0.0, -TopGrowth)));

	Space->RemoveFromParent();
	auto* HelpSlot = Canvas->AddChildToCanvas(Space);
	HelpSlot->SetZOrder(FMath::Max(PageSlot->GetZOrder(), HintSlot->GetZOrder()) + 1);
	WriteRect(*HelpSlot, Help);
	UE_LOG(LogTemp, Display, TEXT("Settings layout: panel height %.0f, page height %.0f, help y %.0f-%.0f, navigation y %.0f"),
		Panel.GetSize().Y, Content.GetSize().Y, Help.Min.Y, Help.Max.Y, Navigation.Min.Y);
	return true;
}

bool AuthorSettingsHelp(UWidgetTree* Tree)
{
	auto* Pages = Tree->FindWidget(TEXT("SettingsTabPages"));
	auto* Container = Pages ? Cast<UVerticalBox>(Pages->GetParent()) : nullptr;
	if (!Container) return false;

	const FText DisplayHelp[] = {
		NSLOCTEXT("SettingsHelp", "Brightness", "어두운 곳에서 발판 윤곽이 희미하게 보이도록 조절합니다."),
		NSLOCTEXT("SettingsHelp", "WindowMode", "전체 화면, 테두리 없는 창, 창 모드 중에서 선택합니다."),
		NSLOCTEXT("SettingsHelp", "Resolution", "화면 해상도를 고릅니다. 테두리 없는 창은 바탕 화면 해상도입니다."),
		NSLOCTEXT("SettingsHelp", "VSync", "화면 갱신에 맞춰 프레임을 표시해 화면 찢어짐을 줄입니다."),
		NSLOCTEXT("SettingsHelp", "FrameLimit", "초당 표시하는 프레임 수의 상한을 정합니다."),
		NSLOCTEXT("SettingsHelp", "Shake", "전투나 충격으로 화면이 흔들리는 강도를 조절합니다."),
		NSLOCTEXT("SettingsHelp", "Bloom", "밝은 물체 주변에 빛이 번지는 효과를 켜거나 끕니다."),
		NSLOCTEXT("SettingsHelp", "ReduceFlashes", "피격과 에너지 무기 발사 시 번쩍임을 줄입니다."),
		NSLOCTEXT("SettingsHelp", "ApplyScreen", "선택한 화면 모드와 해상도를 적용합니다."),
		NSLOCTEXT("SettingsHelp", "QualityPreset", "전체 그래픽 품질을 정합니다. 낮을수록 프레임이 올라갑니다."),
		NSLOCTEXT("SettingsHelp", "RenderScale", "3D 화면을 그리는 해상도입니다. 낮추면 빨라지지만 흐려집니다."),
		NSLOCTEXT("SettingsHelp", "ColorVision", "색약 유형에 맞춰 화면 전체의 색을 보정합니다."),
		NSLOCTEXT("SettingsHelp", "ColorVisionStrength", "색 보정의 세기를 조절합니다.")
	};
	static_assert(UE_ARRAY_COUNT(DisplayHelp) == UE_ARRAY_COUNT(DisplayHelpControls));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DisplayHelpControls); ++Index)
		if (auto* Control = Tree->FindWidget(DisplayHelpControls[Index]); Control && Control->GetToolTipText().IsEmpty())
			Control->SetToolTipText(DisplayHelp[Index]);
	const FText AppearanceHelp[] = {
		NSLOCTEXT("SettingsHelp", "OutlineColor", "외곽선 색상을 고릅니다. 사용자 지정으로 팔레트를 엽니다."),
		NSLOCTEXT("SettingsHelp", "OutlineWidth", "플레이어 외곽선의 굵기를 조절합니다."),
		NSLOCTEXT("SettingsHelp", "OutlineBrightness", "플레이어 외곽선이 얼마나 밝게 빛나는지 조절합니다."),
		NSLOCTEXT("SettingsHelp", "PlayerColor", "본체 색상을 고릅니다. 사용자 지정으로 팔레트를 엽니다."),
		NSLOCTEXT("SettingsHelp", "ResetAppearance", "플레이어 색상과 외곽선 설정을 기본값으로 되돌립니다."),
		NSLOCTEXT("SettingsHelp", "AppearancePreview", "현재 적용한 플레이어 색상과 외곽선을 확인합니다.")
	};
	static_assert(UE_ARRAY_COUNT(AppearanceHelp) == UE_ARRAY_COUNT(AppearanceHelpControls));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AppearanceHelpControls); ++Index)
		if (auto* Control = Tree->FindWidget(AppearanceHelpControls[Index]); Control && Control->GetToolTipText().IsEmpty())
			Control->SetToolTipText(AppearanceHelp[Index]);
	if (auto* Audio = Tree->FindWidget(TEXT("AudioSettingsSection")))
		Audio->SetToolTipText(NSLOCTEXT("SettingsHelp", "Volume", "음량을 조절합니다. 손잡이를 클릭하면 음소거하거나 되돌립니다."));
	if (auto* Reset = Tree->FindWidget(TEXT("ResetGameplayKeys")))
		Reset->SetToolTipText(NSLOCTEXT("SettingsHelp", "ResetKeys", "변경한 모든 키를 기본값으로 되돌립니다."));
	for (const auto& Binding : UGameKeySettingsSubsystem::GetDefaultBindings())
	{
		auto* Selector = Tree->FindWidget(Binding.WidgetName);
		if (!Selector) continue;
		if (Binding.WidgetName.ToString().StartsWith(TEXT("IA_Drop_")))
			Selector->SetToolTipText(NSLOCTEXT("SettingsHelp", "Drop", "아래 키 + 점프키로 하향 점프합니다. 클릭하면 아래 키를 바꿉니다."));
		else if (Selector->GetToolTipText().IsEmpty())
			Selector->SetToolTipText(NSLOCTEXT("SettingsHelp", "CaptureKey", "클릭한 뒤 새 키를 누릅니다. Esc를 누르면 취소됩니다."));
	}
	if (auto* Existing = Tree->FindWidget(TEXT("SettingsHelpText")))
	{
		const auto* ExistingFrame = Existing->GetParent();
		return PlaceSettingsHelpInCanvas(Tree, ExistingFrame ? Cast<USizeBox>(ExistingFrame->GetParent()) : nullptr);
	}

	auto* Frame = Tree->ConstructWidget<UBorder>();
	Frame->SetBrush(FactoryUI::Chamfer());
	Frame->SetPadding(FMargin(14.f, 5.f));
	Frame->SetVerticalAlignment(VAlign_Center);
	Frame->SetVisibility(ESlateVisibility::Hidden);
	auto* Text = MakeSettingsLabel(Tree, FText::GetEmpty(), 22);
	Text->Rename(TEXT("SettingsHelpText"), Tree);
	Text->SetAutoWrapText(true);
	Text->SetRenderTranslation(FVector2D(0.f, -4.f));
	Frame->SetContent(Text);
	auto* Space = Tree->ConstructWidget<USizeBox>();
	Space->SetHeightOverride(62.f);
	Space->SetContent(Frame);
	return PlaceSettingsHelpInCanvas(Tree, Space);
}

void CopyNamedProperties(UObject* Target, const UObject* Source, std::initializer_list<const TCHAR*> Names)
{
	// 디자이너에서 꾸민 스타일을 그대로 옮긴다. 일부 프로퍼티는 setter가 없거나 위젯 생성 전에만 바꿀 수 있어서 리플렉션으로 복사한다.
	if (!Target || !Source || !Target->IsA(Source->GetClass())) return;
	for (const TCHAR* Name : Names)
		if (const FProperty* Property = Source->GetClass()->FindPropertyByName(Name))
			Property->CopyCompleteValue_InContainer(Target, Source);
}

void CopyTextLook(UTextBlock* Target, const UTextBlock* Source)
{
	if (!Target || !Source) return;
	Target->SetFont(Source->GetFont());
	Target->SetColorAndOpacity(Source->GetColorAndOpacity());
	// UTextBlock에는 정렬 getter가 없어서 리플렉션으로 복사한다.
	CopyNamedProperties(Target, Source, {TEXT("Justification")});
	Target->SetRenderTransform(Source->GetRenderTransform());
	Target->SetVisibility(Source->GetVisibility());
}

/** Column의 끝에 밝기 줄과 같은 크기, 라벨 폭, 글꼴, 여백으로 설정 줄을 하나 추가한다. */
bool AddRowLikeBrightness(UWidgetTree* Tree, UVerticalBox* Column, const FText& Caption, UWidget* Control)
{
	auto* TemplateControl = Tree->FindWidget(TEXT("BrightnessSetting"));
	auto* TemplateLine = TemplateControl ? Cast<UHorizontalBox>(TemplateControl->GetParent()) : nullptr;
	auto* TemplateRow = TemplateLine ? Cast<USizeBox>(TemplateLine->GetParent()) : nullptr;
	auto* TemplateLabelBox = TemplateLine ? Cast<USizeBox>(TemplateLine->GetChildAt(0)) : nullptr;
	auto* TemplateLabel = TemplateLabelBox ? Cast<UTextBlock>(TemplateLabelBox->GetContent()) : nullptr;
	const auto* TemplateRowSlot = TemplateRow ? Cast<UVerticalBoxSlot>(TemplateRow->Slot) : nullptr;
	const auto* TemplateControlSlot = Cast<UHorizontalBoxSlot>(TemplateControl ? TemplateControl->Slot : nullptr);
	if (!Column || !Control || !TemplateLabel || !TemplateRowSlot || !TemplateControlSlot) return false;

	auto* Row = Tree->ConstructWidget<USizeBox>();
	Row->SetHeightOverride(TemplateRow->GetHeightOverride());
	auto* Line = Tree->ConstructWidget<UHorizontalBox>();
	Row->SetContent(Line);
	auto* LabelBox = Tree->ConstructWidget<USizeBox>();
	LabelBox->SetWidthOverride(TemplateLabelBox->GetWidthOverride());
	auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Control->GetName() + TEXT("Label")));
	CopyTextLook(Label, TemplateLabel);
	Label->SetText(Caption);
	LabelBox->SetContent(Label);
	auto* LabelSlot = Line->AddChildToHorizontalBox(LabelBox);
	if (const auto* TemplateLabelSlot = Cast<UHorizontalBoxSlot>(TemplateLabelBox->Slot))
	{
		LabelSlot->SetPadding(TemplateLabelSlot->GetPadding());
		LabelSlot->SetVerticalAlignment(TemplateLabelSlot->GetVerticalAlignment());
	}
	auto* ControlSlot = Line->AddChildToHorizontalBox(Control);
	ControlSlot->SetSize(TemplateControlSlot->GetSize());
	ControlSlot->SetPadding(TemplateControlSlot->GetPadding());
	ControlSlot->SetHorizontalAlignment(TemplateControlSlot->GetHorizontalAlignment());
	ControlSlot->SetVerticalAlignment(TemplateControlSlot->GetVerticalAlignment());
	Column->AddChildToVerticalBox(Row)->SetPadding(TemplateRowSlot->GetPadding());
	return true;
}

/** 화면 모드 드롭다운의 디자이너 스타일을 가진 새 드롭다운 */
UComboBoxString* MakeComboLikeWindowMode(UWidgetTree* Tree, FName Name)
{
	auto* Combo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), Name);
	CopyNamedProperties(Combo, Tree->FindWidget(TEXT("WindowModeSetting")), {TEXT("WidgetStyle"), TEXT("ItemStyle"),
		TEXT("ScrollBarStyle"), TEXT("ContentPadding"), TEXT("MaxListHeight"), TEXT("HasDownArrow"),
		TEXT("EnableGamepadNavigationMode"), TEXT("Font"), TEXT("ForegroundColor"), TEXT("bIsFocusable")});
	return Combo;
}

/** 밝기 슬라이더의 디자이너 스타일을 가진 새 슬라이더 */
USlider* MakeSliderLikeBrightness(UWidgetTree* Tree, FName Name)
{
	auto* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
	CopyNamedProperties(Slider, Tree->FindWidget(TEXT("BrightnessSetting")), {TEXT("WidgetStyle"), TEXT("SliderBarColor"),
		TEXT("SliderHandleColor"), TEXT("IndentHandle"), TEXT("MouseUsesStep"), TEXT("RequiresControllerLock"),
		TEXT("IsFocusable"), TEXT("Orientation")});
	return Slider;
}

/** 그래픽 탭과 소리 탭을 나누고 그래픽 품질, 렌더링 해상도, 색각 보정 줄을 넣는다. 여러 번 실행해도 결과가 같다. */
bool AuthorGraphicsAndAudioPages(UWidgetTree* Tree)
{
	auto* Pages = Cast<UWidgetSwitcher>(Tree->FindWidget(TEXT("SettingsTabPages")));
	auto* Columns = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("SettingsColumns")));
	auto* Display = Cast<UVerticalBox>(Tree->FindWidget(TEXT("VisualSettingsColumn")));
	auto* GeneralTab = Cast<UButton>(Tree->FindWidget(TEXT("GeneralSettingsTab")));
	auto* GeneralCaption = Cast<UTextBlock>(Tree->FindWidget(TEXT("GeneralSettingsTabText")));
	auto* GeneralTabBox = GeneralTab ? Cast<USizeBox>(GeneralTab->GetParent()) : nullptr;
	auto* Tabs = GeneralTabBox ? Cast<UHorizontalBox>(GeneralTabBox->GetParent()) : nullptr;
	if (!Pages || !Columns || !Display || !GeneralTab || !GeneralCaption || !Tabs)
	{
		UE_LOG(LogTemp, Error, TEXT("Settings layout: graphics/audio split needs SettingsTabPages, SettingsColumns, VisualSettingsColumn and GeneralSettingsTab."));
		return false;
	}

	// 1. 탭: "화면 및 소리" -> "그래픽", 바로 뒤에 "소리"
	GeneralCaption->SetText(NSLOCTEXT("Settings", "GraphicsTab", "그래픽"));
	if (!Tree->FindWidget(TEXT("AudioSettingsTab")))
	{
		auto* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(GeneralTabBox->GetWidthOverride());
		Size->SetHeightOverride(GeneralTabBox->GetHeightOverride());
		auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AudioSettingsTab"));
		Button->SetStyle(GeneralTab->GetStyle());
		Button->SetColorAndOpacity(GeneralTab->GetColorAndOpacity());
		Button->SetBackgroundColor(GeneralTab->GetBackgroundColor());
		Size->SetContent(Button);
		auto* Caption = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AudioSettingsTabText"));
		CopyTextLook(Caption, GeneralCaption);
		Caption->SetText(NSLOCTEXT("Settings", "AudioTab", "소리"));
		auto* CaptionSlot = CastChecked<UButtonSlot>(Button->AddChild(Caption));
		if (const auto* GeneralCaptionSlot = Cast<UButtonSlot>(GeneralCaption->Slot))
		{
			CaptionSlot->SetPadding(GeneralCaptionSlot->GetPadding());
			CaptionSlot->SetHorizontalAlignment(GeneralCaptionSlot->GetHorizontalAlignment());
			CaptionSlot->SetVerticalAlignment(GeneralCaptionSlot->GetVerticalAlignment());
		}
		auto* TabSlot = Cast<UHorizontalBoxSlot>(Tabs->InsertChildAt(Tabs->GetChildIndex(GeneralTabBox) + 1, Size));
		if (const auto* GeneralSlot = Cast<UHorizontalBoxSlot>(GeneralTabBox->Slot); TabSlot && GeneralSlot)
		{
			TabSlot->SetPadding(GeneralSlot->GetPadding());
			TabSlot->SetSize(GeneralSlot->GetSize());
			TabSlot->SetHorizontalAlignment(GeneralSlot->GetHorizontalAlignment());
			TabSlot->SetVerticalAlignment(GeneralSlot->GetVerticalAlignment());
		}
	}

	// 2. 소리 페이지: 그래픽 페이지 오른쪽에 있던 볼륨 섹션을 독립 페이지(1번)로 옮긴다.
	if (!Tree->FindWidget(TEXT("AudioSettingsPage")))
	{
		UWidget* Audio = Tree->FindWidget(TEXT("AudioSettingsSection"));
		if (!Audio)
		{
			UE_LOG(LogTemp, Error, TEXT("Settings layout: AudioSettingsSection not found."));
			return false;
		}
		FMargin Padding;
		if (const auto* OldSlot = Cast<UHorizontalBoxSlot>(Audio->Slot)) Padding = OldSlot->GetPadding();
		Audio->RemoveFromParent();
		auto* Page = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AudioSettingsPage"));
		auto* Width = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AudioSettingsWidth"));
		// 페이지 전체 폭으로 늘리면 슬라이더가 지나치게 길어져서 폭을 고정하고 왼쪽(다른 탭과 같은 시작선)에 둔다.
		// 최대 폭(MaxDesiredWidth)만 주면 슬라이더의 원하는 크기가 작아서 줄어들므로 고정 폭을 쓴다. 디자이너에서 바꿔도 된다.
		Width->SetWidthOverride(760.f);
		Width->SetContent(Audio);
		auto* WidthSlot = Page->AddChildToHorizontalBox(Width);
		WidthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		WidthSlot->SetHorizontalAlignment(HAlign_Left);
		WidthSlot->SetVerticalAlignment(VAlign_Top);
		WidthSlot->SetPadding(Padding);
		Pages->InsertChildAt(AudioPage, Page);
	}

	// 3. 그래픽 페이지 오른쪽 열: 품질 및 효과
	if (!Tree->FindWidget(TEXT("QualitySettingsColumn")))
	{
		const auto FindRow = [&](FName ControlName, UPanelWidget* Column) -> UWidget*
		{
			UWidget* Row = Tree->FindWidget(ControlName);
			while (Row && Row->GetParent() && Row->GetParent() != Column) Row = Row->GetParent();
			return Row && Row->GetParent() == Column ? Row : nullptr;
		};
		// 새 줄은 밝기 줄의 크기, 라벨 폭, 글꼴, 여백을 그대로 따른다.
		auto* TemplateControl = Cast<USlider>(Tree->FindWidget(TEXT("BrightnessSetting")));
		auto* TemplateRow = Cast<USizeBox>(FindRow(TEXT("BrightnessSetting"), Display));
		auto* TemplateLine = TemplateControl ? Cast<UHorizontalBox>(TemplateControl->GetParent()) : nullptr;
		auto* TemplateLabelBox = TemplateLine ? Cast<USizeBox>(TemplateLine->GetChildAt(0)) : nullptr;
		auto* TemplateLabel = TemplateLabelBox ? Cast<UTextBlock>(TemplateLabelBox->GetContent()) : nullptr;
		auto* TemplateCombo = Cast<UComboBoxString>(Tree->FindWidget(TEXT("WindowModeSetting")));
		auto* DisplayHeading = Cast<UTextBlock>(Display->GetChildAt(0));
		if (!TemplateControl || !TemplateRow || !TemplateLabelBox || !TemplateLabel || !TemplateCombo || !DisplayHeading)
		{
			UE_LOG(LogTemp, Error, TEXT("Settings layout: brightness row, window mode dropdown or display heading not found."));
			return false;
		}

		auto* Quality = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QualitySettingsColumn"));
		auto* QualitySlot = Columns->AddChildToHorizontalBox(Quality);
		if (const auto* DisplaySlot = Cast<UHorizontalBoxSlot>(Display->Slot))
		{
			const FMargin Padding = DisplaySlot->GetPadding();
			QualitySlot->SetPadding(FMargin(Padding.Right, Padding.Top, Padding.Left, Padding.Bottom));
			QualitySlot->SetSize(DisplaySlot->GetSize());
			QualitySlot->SetHorizontalAlignment(DisplaySlot->GetHorizontalAlignment());
			QualitySlot->SetVerticalAlignment(DisplaySlot->GetVerticalAlignment());
		}

		auto* Heading = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QualityHeading"));
		CopyTextLook(Heading, DisplayHeading);
		Heading->SetText(NSLOCTEXT("Settings", "QualityHeading", "품질 및 효과"));
		auto* HeadingSlot = Quality->AddChildToVerticalBox(Heading);
		if (const auto* DisplayHeadingSlot = Cast<UVerticalBoxSlot>(DisplayHeading->Slot))
		{
			HeadingSlot->SetPadding(DisplayHeadingSlot->GetPadding());
			HeadingSlot->SetHorizontalAlignment(DisplayHeadingSlot->GetHorizontalAlignment());
		}

		const auto AddRow = [&](const FText& Caption, UWidget* Control)
		{
			AddRowLikeBrightness(Tree, Quality, Caption, Control);
		};

		AddRow(NSLOCTEXT("Settings", "QualityPreset", "그래픽 품질"), MakeComboLikeWindowMode(Tree, TEXT("QualityPresetSetting")));
		AddRow(NSLOCTEXT("Settings", "RenderScale", "렌더링 해상도"), MakeSliderLikeBrightness(Tree, TEXT("RenderScaleSetting")));

		// 화면 열에 있던 효과 줄을 품질 열로 옮긴다. 화면 열에는 화면 모드 적용 버튼까지만 남는다.
		for (const TCHAR* Name : {TEXT("BloomSetting"), TEXT("ShakeSetting"), TEXT("ReduceFlashesSetting")})
		{
			if (UWidget* Row = FindRow(Name, Display))
			{
				const FMargin Padding = CastChecked<UVerticalBoxSlot>(Row->Slot)->GetPadding();
				Row->RemoveFromParent();
				Quality->AddChildToVerticalBox(Row)->SetPadding(Padding);
			}
		}
	}
	// 4. 색각 보정: 품질 및 효과 열 끝에 드롭다운과 세기 슬라이더
	if (!Tree->FindWidget(TEXT("ColorVisionSetting")))
	{
		auto* Quality = Cast<UVerticalBox>(Tree->FindWidget(TEXT("QualitySettingsColumn")));
		if (!Quality
			|| !AddRowLikeBrightness(Tree, Quality, NSLOCTEXT("Settings", "ColorVision", "색각 보정"),
				MakeComboLikeWindowMode(Tree, TEXT("ColorVisionSetting")))
			|| !AddRowLikeBrightness(Tree, Quality, NSLOCTEXT("Settings", "ColorVisionStrength", "보정 강도"),
				MakeSliderLikeBrightness(Tree, TEXT("ColorVisionStrengthSetting"))))
		{
			UE_LOG(LogTemp, Error, TEXT("Settings layout: could not add the color vision rows."));
			return false;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Settings layout: graphics and audio pages are separate (%d pages)."), Pages->GetChildrenCount());
	return true;
}

bool AuthorKeySettingsGroups(UWidgetTree* Tree)
{
	if (Tree->FindWidget(TEXT("KeySettingsMovementHeading"))) return true;

	struct FKeyRow
	{
		UWidget* Widget;
		int32 Order;
	};
	TArray<FKeyRow> MovementRows;
	TArray<FKeyRow> CombatRows;
	TArray<FKeyRow> UIRows;
	UHorizontalBox* Columns = nullptr;
	for (const auto& Binding : UGameKeySettingsSubsystem::GetDefaultBindings())
	{
		UWidget* Row = Tree->FindWidget(Binding.WidgetName);
		while (Row && !Cast<UVerticalBox>(Row->GetParent())) Row = Row->GetParent();
		if (!Row) continue;
		if (!Columns) Columns = Cast<UHorizontalBox>(Row->GetParent()->GetParent());

		const FString Name = Binding.WidgetName.ToString();
		if (Name.StartsWith(TEXT("IA_Move_"))) MovementRows.Add({Row, 0});
		else if (Name.StartsWith(TEXT("IA_Jump_"))) MovementRows.Add({Row, 1});
		else if (Name.StartsWith(TEXT("IA_Drop_"))) MovementRows.Add({Row, 2});
		else if (Name.StartsWith(TEXT("IA_GrapplingHook_"))) MovementRows.Add({Row, 3});
		else if (Name.StartsWith(TEXT("IA_Interact_")) || Name.StartsWith(TEXT("IA_Map_")) || Binding.MappingIndex == INDEX_NONE)
			UIRows.Add({Row, 0});
		else
			CombatRows.Add({Row, Name.StartsWith(TEXT("IA_Weapon")) ? 1 : 0});
	}
	auto* Left = Columns ? Cast<UVerticalBox>(Columns->GetChildAt(0)) : nullptr;
	auto* Right = Columns ? Cast<UVerticalBox>(Columns->GetChildAt(1)) : nullptr;
	if (!Left || !Right) return false;
	for (const auto* Group : {&MovementRows, &CombatRows, &UIRows})
		for (const auto& Row : *Group)
			Row.Widget->RemoveFromParent();

	const auto AddGroup = [&](UVerticalBox* Column, FName HeadingName, const FText& Caption, TArray<FKeyRow>& Rows)
	{
		auto* Heading = MakeSettingsLabel(Tree, Caption, 18);
		Heading->Rename(*HeadingName.ToString(), Tree);
		Heading->SetColorAndOpacity(FactoryUI::Cyan);
		Heading->SetRenderTranslation(FVector2D(0.f, -3.f));
		Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.f, 3.f, 0.f, 4.f));
		Rows.StableSort([](const FKeyRow& A, const FKeyRow& B) { return A.Order < B.Order; });
		for (const auto& Row : Rows)
			Column->AddChildToVerticalBox(Row.Widget)->SetPadding(FMargin(0.f, 2.f));
	};
	AddGroup(Left, TEXT("KeySettingsMovementHeading"), NSLOCTEXT("KeySettings", "MovementGroup", "이동"), MovementRows);
	AddGroup(Left, TEXT("KeySettingsUIHeading"), NSLOCTEXT("KeySettings", "UIGroup", "UI"), UIRows);
	AddGroup(Right, TEXT("KeySettingsCombatHeading"), NSLOCTEXT("KeySettings", "CombatGroup", "전투"), CombatRows);
	return true;
}

bool AuthorKeySettingsTypography(UWidgetTree* Tree)
{
	const auto* FontAsset = GetDefault<UFactoryUIAssetSettings>()->BodyFont.LoadSynchronous();
	if (!FontAsset) return false;
	const FSlateFontInfo KeyFont(FontAsset, 17, TEXT("Regular"));
	Tree->ForEachWidget([&](UWidget* Widget)
	{
		if (auto* Selector = Cast<UInputKeySelector>(Widget))
		{
			auto Style = Selector->GetTextStyle();
			Style.SetFont(KeyFont);
			Selector->SetTextStyle(Style);
			Selector->SetMargin(FMargin(6.f, -4.f, 6.f, 4.f));
		}
	});
	if (auto* Reset = Cast<UTextBlock>(Tree->FindWidget(TEXT("ResetGameplayKeysText"))))
	{
		Reset->SetFont(KeyFont);
		Reset->SetRenderTranslation(FVector2D(0.f, -4.f));
	}
	if (auto* Jump = Cast<UTextBlock>(Tree->FindWidget(TEXT("DropJumpKey"))))
		Jump->SetText(NSLOCTEXT("KeySettings", "DropJumpKeyLabel", "+ 점프키"));
	return true;
}
}

bool UAudioSettingsWidget::AuthorSettingsLayout(UWidgetTree* WidgetTree)
{
	// 하위 탭을 별도 WBP로 나눈 뒤에는 각 페이지 WBP를 디자이너에서 직접 고친다. 이 자동 구성은 나누기 전 구조에만 쓴다.
	if (WidgetTree->FindWidget(TEXT("GraphicsSettingsSection")))
	{
		UE_LOG(LogTemp, Display, TEXT("Settings layout: tab pages are separate Widget Blueprints. Edit WBP_GraphicsSettings, WBP_AudioSettings, WBP_AppearanceSettings and WBP_KeySettings directly."));
		return true;
	}
	auto* Display = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("VisualSettingsColumn")));
	if (!Display) return false;
	BuildVisualLayout(WidgetTree, Display);
	if (WidgetTree->FindWidget(TEXT("SettingsTabPages")))
		return AuthorGraphicsAndAudioPages(WidgetTree) && AuthorKeySettingsGroups(WidgetTree)
			&& AuthorKeySettingsTypography(WidgetTree) && AuthorSettingsHelp(WidgetTree);
	auto* Columns = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("SettingsColumns")));
	UPanelWidget* Parent = Columns ? Columns->GetParent() : nullptr;
	if (!Parent) return false;

	auto* Container = WidgetTree->ConstructWidget<UVerticalBox>();
	const int32 ColumnsIndex = Parent->GetChildIndex(Columns);
	UPanelSlot* ColumnsSlot = Columns->Slot;
	Parent->RemoveChild(Columns);
	Parent->InsertChildAt(ColumnsIndex, Container, ColumnsSlot);

	auto* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	Container->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
	auto AddTab = [&](int32 Index, const FText& Caption)
	{
		auto* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetHeightOverride(40.f);
		Size->SetWidthOverride(174.f);
		Tabs->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
		auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), SettingsTabs[Index].Button);
		Size->SetContent(Button);
		FButtonStyle Style;
		const FVector4 Corners(9.f, 9.f, 0.f, 0.f);
		Style.Normal = FSlateRoundedBoxBrush(FactoryUI::Panel, Corners, FactoryUI::Edge, 1.f);
		Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.09f, 0.18f, 0.21f), Corners, FactoryUI::Cyan, 1.f);
		Style.Pressed = Style.Hovered;
		Style.Disabled = Style.Normal;
		Button->SetStyle(Style);
		auto* Text = MakeSettingsLabel(WidgetTree, Caption, 18);
		Text->Rename(SettingsTabs[Index].Text, WidgetTree);
		Text->SetJustification(ETextJustify::Center);
		Text->SetRenderTranslation(FVector2D(0.f, -4.f));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* LabelSlot = CastChecked<UButtonSlot>(Button->AddChild(Text));
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	};
	// 소리 탭(1)은 AuthorGraphicsAndAudioPages가 그래픽 탭 바로 뒤에 넣는다.
	AddTab(0, NSLOCTEXT("Settings", "GraphicsTab", "그래픽"));
	AddTab(2, NSLOCTEXT("Settings", "AppearanceTab", "플레이어 외형"));
	AddTab(3, NSLOCTEXT("Settings", "KeysTab", "키 설정"));
	auto* TabLine = WidgetTree->ConstructWidget<UImage>();
	auto TabLineBrush = FactoryUI::Plate(FactoryUI::Edge, FactoryUI::Edge, 0.f);
	TabLineBrush.ImageSize = FVector2f(1.f, 1.f);
	TabLine->SetBrush(TabLineBrush);
	Container->AddChildToVerticalBox(TabLine)->SetPadding(FMargin(4.f, 0.f, 4.f, 12.f));

	auto* Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("SettingsTabPages"));
	Container->AddChildToVerticalBox(Pages)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Pages->AddChild(Columns);
	auto* AppearanceColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Pages->AddChild(AppearanceColumns);
	auto* Appearance = WidgetTree->ConstructWidget<UVerticalBox>();
	auto* AppearanceSlot = AppearanceColumns->AddChildToHorizontalBox(Appearance);
	AppearanceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AppearanceSlot->SetVerticalAlignment(VAlign_Top);
	AppearanceSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	auto* Heading = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("VisualHeading")));
	if (!Heading) Heading = Cast<UTextBlock>(Display->GetChildAt(0));
	if (Heading)
		Heading->SetText(NSLOCTEXT("Settings", "DisplayHeading", "화면"));

	const auto MoveRow = [&](FName ControlName, const FText& Caption, UClass* ControlClass)
	{
		UWidget* Row = WidgetTree->FindWidget(ControlName);
		while (Row && Row->GetParent() && Row->GetParent() != Display) Row = Row->GetParent();
		if (Row && Row->GetParent() == Display)
		{
			Row->RemoveFromParent();
			Appearance->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 2));
		}
		else if (!WidgetTree->FindWidget(ControlName))
		{
			AddSettingsRow(WidgetTree, Appearance, Caption, WidgetTree->ConstructWidget<UWidget>(ControlClass, ControlName));
		}
	};
	const FName OutlineColorName(TEXT("OutlineColorSetting"));
	auto* AuthoredCombo = Cast<UComboBoxString>(WidgetTree->FindWidget(OutlineColorName));
	auto* ColorRow = AuthoredCombo ? Cast<UHorizontalBox>(AuthoredCombo->GetParent()) : nullptr;
	if (ColorRow && !AuthoredCombo->IsA<UAppearanceColorComboBox>())
	{
		const auto* OldSlot = CastChecked<UHorizontalBoxSlot>(AuthoredCombo->Slot);
		const int32 SlotIndex = ColorRow->GetChildIndex(AuthoredCombo);
		const FName OldName = MakeUniqueObjectName(WidgetTree, AuthoredCombo->GetClass(), OutlineColorName);
		AuthoredCombo->Rename(*OldName.ToString(), WidgetTree, REN_DontCreateRedirectors | REN_DoNotDirty);
		auto* Combo = WidgetTree->ConstructWidget<UAppearanceColorComboBox>(UAppearanceColorComboBox::StaticClass(), OutlineColorName);
		Combo->CopyStyleFrom(*AuthoredCombo);
		ColorRow->RemoveChild(AuthoredCombo);
		auto* ComboSlot = ColorRow->AddChildToHorizontalBox(Combo);
		ComboSlot->SetSize(OldSlot->GetSize());
		ComboSlot->SetPadding(OldSlot->GetPadding());
		ComboSlot->SetHorizontalAlignment(OldSlot->GetHorizontalAlignment());
		ComboSlot->SetVerticalAlignment(OldSlot->GetVerticalAlignment());
		ColorRow->ShiftChild(SlotIndex, Combo);
	}
	MoveRow(OutlineColorName, NSLOCTEXT("Settings", "OutlineColor", "외곽선 색상"), UAppearanceColorComboBox::StaticClass());
	MoveRow(TEXT("OutlineWidthSetting"), NSLOCTEXT("Settings", "OutlineWidth", "외곽선 굵기"), USlider::StaticClass());
	AddSettingsRow(WidgetTree, Appearance, NSLOCTEXT("Settings", "OutlineBrightness", "외곽선 밝기"),
		WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("OutlineBrightnessSetting")));
	const FName PlayerColorName(TEXT("PlayerColorSetting"));
	auto* PlayerColor = WidgetTree->ConstructWidget<UAppearanceColorComboBox>(UAppearanceColorComboBox::StaticClass(), PlayerColorName);
	if (const auto* OutlineColor = Cast<UComboBoxString>(WidgetTree->FindWidget(OutlineColorName)))
		PlayerColor->CopyStyleFrom(*OutlineColor);
	AddSettingsRow(WidgetTree, Appearance, NSLOCTEXT("Settings", "PlayerColor", "플레이어 색상"), PlayerColor);
	for (int32 Index = 0; Index < Appearance->GetChildrenCount(); ++Index)
	{
		auto* Row = Appearance->GetChildAt(Index);
		CastChecked<UVerticalBoxSlot>(Row->Slot)->SetPadding(FMargin(0.f, 6.f));
	}
	auto* ResetRow = WidgetTree->ConstructWidget<UVerticalBox>();
	Appearance->AddChildToVerticalBox(ResetRow)->SetHorizontalAlignment(HAlign_Right);
	FactoryMenuStyle::AddButton(WidgetTree, ResetRow, TEXT("ResetAppearanceButton"), TEXT("ResetAppearanceText"),
		NSLOCTEXT("Settings", "ResetAppearance", "기본값"), true);
	auto* ResetButton = CastChecked<UButton>(WidgetTree->FindWidget(TEXT("ResetAppearanceButton")));
	CastChecked<USizeBox>(ResetButton->GetParent())->SetHeightOverride(36.f);
	auto* ResetText = CastChecked<UTextBlock>(WidgetTree->FindWidget(TEXT("ResetAppearanceText")));
	auto Font = ResetText->GetFont();
	Font.Size = 19;
	ResetText->SetFont(Font);
	CastChecked<UButtonSlot>(ResetText->Slot)->SetVerticalAlignment(VAlign_Center);

	{
		auto* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrush(FactoryUI::Chamfer());
		Frame->SetPadding(FMargin(8.f));
		auto* PreviewBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AppearancePreviewBox"));
		PreviewBox->SetWidthOverride(370.f);
		PreviewBox->SetHeightOverride(420.f);
		auto* Preview = WidgetTree->ConstructWidget<UViewport>(UViewport::StaticClass(), TEXT("AppearancePreview"));
		Preview->SetBackgroundColor(FLinearColor::FromSRGBColor(PreviewBackgroundEdge));
		Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
		PreviewBox->SetContent(Preview);
		Frame->SetContent(PreviewBox);
		auto* PreviewSlot = AppearanceColumns->AddChildToHorizontalBox(Frame);
		PreviewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PreviewSlot->SetHorizontalAlignment(HAlign_Center);
		PreviewSlot->SetVerticalAlignment(VAlign_Center);
		PreviewSlot->SetPadding(FMargin(8.f, 6.f));
	}
	AuthorKeySettingsPage(WidgetTree, Pages);
	return AuthorGraphicsAndAudioPages(WidgetTree) && AuthorKeySettingsGroups(WidgetTree)
		&& AuthorKeySettingsTypography(WidgetTree) && AuthorSettingsHelp(WidgetTree);
}

bool UAudioSettingsWidget::AuthorLanguageTab(UWidgetTree* Tree)
{
	if (Tree->FindWidget(TEXT("LanguageSettingsTab"))) return true;
	auto* KeyTab = Cast<UButton>(Tree->FindWidget(TEXT("KeySettingsTab")));
	auto* KeyCaption = Cast<UTextBlock>(Tree->FindWidget(TEXT("KeySettingsTabText")));
	auto* KeyBox = KeyTab ? Cast<USizeBox>(KeyTab->GetParent()) : nullptr;
	auto* Tabs = KeyBox ? Cast<UHorizontalBox>(KeyBox->GetParent()) : nullptr;
	const auto* KeyCaptionSlot = KeyCaption ? Cast<UButtonSlot>(KeyCaption->Slot) : nullptr;
	const auto* KeyBoxSlot = KeyBox ? Cast<UHorizontalBoxSlot>(KeyBox->Slot) : nullptr;
	if (!Tabs || !KeyCaptionSlot || !KeyBoxSlot)
	{
		UE_LOG(LogTemp, Error, TEXT("Settings layout: the language tab needs KeySettingsTab inside a SizeBox in the tab row, with KeySettingsTabText as its label."));
		return false;
	}

	// 지금 탭 줄이 차지하는 폭(탭 폭 + 여백)을 다섯 칸으로 다시 나눈다.
	const auto TabSpace = [](const UWidget* Tab)
	{
		const auto* Box = Cast<USizeBox>(Tab);
		const auto* BoxSlot = Cast<UHorizontalBoxSlot>(Tab->Slot);
		const FMargin Padding = BoxSlot ? BoxSlot->GetPadding() : FMargin();
		return TPair<float, float>(Box ? Box->GetWidthOverride() : 0.f, Padding.Left + Padding.Right);
	};
	float RowWidth = 0.f;
	for (int32 Index = 0; Index < Tabs->GetChildrenCount(); ++Index)
	{
		const auto Space = TabSpace(Tabs->GetChildAt(Index));
		RowWidth += Space.Key + Space.Value;
	}

	// 키 설정 탭과 같은 모양의 "언어" 탭을 맨 끝에 붙인다.
	auto* Size = Tree->ConstructWidget<USizeBox>();
	Size->SetHeightOverride(KeyBox->GetHeightOverride());
	auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LanguageSettingsTab"));
	Button->SetStyle(KeyTab->GetStyle());
	Button->SetColorAndOpacity(KeyTab->GetColorAndOpacity());
	Button->SetBackgroundColor(KeyTab->GetBackgroundColor());
	Size->SetContent(Button);
	auto* Caption = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LanguageSettingsTabText"));
	CopyTextLook(Caption, KeyCaption);
	Caption->SetText(NSLOCTEXT("Settings", "LanguageTab", "언어"));
	auto* CaptionSlot = CastChecked<UButtonSlot>(Button->AddChild(Caption));
	CaptionSlot->SetPadding(KeyCaptionSlot->GetPadding());
	CaptionSlot->SetHorizontalAlignment(KeyCaptionSlot->GetHorizontalAlignment());
	CaptionSlot->SetVerticalAlignment(KeyCaptionSlot->GetVerticalAlignment());
	auto* TabSlot = Tabs->AddChildToHorizontalBox(Size);
	TabSlot->SetPadding(KeyBoxSlot->GetPadding());
	TabSlot->SetSize(KeyBoxSlot->GetSize());
	TabSlot->SetHorizontalAlignment(KeyBoxSlot->GetHorizontalAlignment());
	TabSlot->SetVerticalAlignment(KeyBoxSlot->GetVerticalAlignment());

	const float PerTab = RowWidth / Tabs->GetChildrenCount();
	for (int32 Index = 0; Index < Tabs->GetChildrenCount(); ++Index)
		if (auto* Box = Cast<USizeBox>(Tabs->GetChildAt(Index)))
			Box->SetWidthOverride(FMath::FloorToFloat(PerTab - TabSpace(Box).Value));

	// 번역하면 탭 이름이 길어질 수 있다(독일어, 러시아어 등). 탭 폭을 넘으면 글자를 줄이도록 ScaleBox로 감싼다.
	for (const FSettingsTab& Tab : SettingsTabs)
	{
		auto* Text = Cast<UTextBlock>(Tree->FindWidget(Tab.Text));
		auto* Owner = Text ? Cast<UButton>(Text->GetParent()) : nullptr;
		const auto* OldSlot = Text ? Cast<UButtonSlot>(Text->Slot) : nullptr;
		if (!Owner || !OldSlot) continue;
		const FMargin Padding = OldSlot->GetPadding();
		const EHorizontalAlignment HAlign = OldSlot->GetHorizontalAlignment();
		const EVerticalAlignment VAlign = OldSlot->GetVerticalAlignment();
		Owner->RemoveChild(Text);
		auto* Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), *(Text->GetName() + TEXT("Fit")));
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		Scale->SetVisibility(ESlateVisibility::HitTestInvisible);
		Scale->SetContent(Text);
		auto* ScaleSlot = CastChecked<UButtonSlot>(Owner->AddChild(Scale));
		ScaleSlot->SetPadding(Padding);
		ScaleSlot->SetHorizontalAlignment(HAlign);
		ScaleSlot->SetVerticalAlignment(VAlign);
	}
	UE_LOG(LogTemp, Display, TEXT("Settings layout: language tab added, %d tabs at %.0f px."), Tabs->GetChildrenCount(), FMath::FloorToFloat(PerTab - TabSpace(Size).Value));
	return true;
}

bool UAudioSettingsWidget::AuthorLanguagePage(UWidgetTree* Tree)
{
	if (!Tree || Tree->RootWidget) return false;
	// 페이지 루트: 다른 페이지와 같은 시작선(왼쪽)에 고정 폭으로 둔다. 디자이너에서 바꿔도 된다.
	auto* Page = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LanguageSettingsPage"));
	Tree->RootWidget = Page;
	auto* Width = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LanguageSettingsWidth"));
	Width->SetWidthOverride(760.f);
	auto* WidthSlot = Page->AddChildToHorizontalBox(Width);
	WidthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WidthSlot->SetHorizontalAlignment(HAlign_Left);
	WidthSlot->SetVerticalAlignment(VAlign_Top);
	auto* Grid = Tree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("LanguageGrid"));
	Grid->SetSlotPadding(FMargin(6.f, 5.f));
	Width->SetContent(Grid);

	// 버튼 모양은 탭과 같은 둥근 상자. 평소 테두리는 FactoryUI::Edge, 마우스/포커스는 청록. 현재 언어 표시는 코드가 덧입힌다.
	FButtonStyle Style;
	Style.Normal = FSlateRoundedBoxBrush(FactoryUI::Panel, 6.f, FactoryUI::Edge, 1.f);
	Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(0.09f, 0.18f, 0.21f), 6.f, FactoryUI::Cyan, 1.f);
	Style.Pressed = Style.Hovered;
	Style.Disabled = Style.Normal;
	Style.NormalPadding = FMargin(14.f, 4.f);
	Style.PressedPadding = Style.NormalPadding;

	// 언어 이름은 어느 언어 화면에서든 보여야 한다. 설정 화면 글꼴(F_TLM_UI)의 Fallback에는 한자가 없어서
	// Fallback이 Noto Sans SC(한자 전체)인 F_TLM_Body로 쓴다. Docs/Fonts.md "다국어" 참고.
	UFont* NameFont = LoadObject<UFont>(nullptr, TEXT("/Game/Main/UI/Fonts/F_TLM_Body.F_TLM_Body"));
	if (!NameFont)
	{
		UE_LOG(LogTemp, Error, TEXT("Settings layout: F_TLM_Body not found."));
		return false;
	}
	const auto UseNameFont = [NameFont](UTextBlock* Text, int32 Size)
	{
		Text->SetFont(FSlateFontInfo(NameFont, Size, TEXT("Medium")));
	};

	const TConstArrayView<FGameLanguage> Languages = UGameLanguageSettingsSubsystem::GetSupportedLanguages();
	for (int32 Index = 0; Index < Languages.Num(); ++Index)
	{
		const FGameLanguage& Language = Languages[Index];
		const FString Name = UGameLanguageSettingsSubsystem::GetLanguageWidgetName(Language).ToString();
		auto* Cell = Tree->ConstructWidget<USizeBox>();
		Cell->SetHeightOverride(56.f);
		auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), *Name);
		Button->SetStyle(Style);
		Cell->SetContent(Button);
		auto* Row = Tree->ConstructWidget<UHorizontalBox>();
		auto* RowSlot = CastChecked<UButtonSlot>(Button->AddChild(Row));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetVerticalAlignment(VAlign_Center);

		// 국기(4:3) 또는 "繁" 배지
		auto* FlagBox = Tree->ConstructWidget<USizeBox>();
		FlagBox->SetWidthOverride(40.f);
		FlagBox->SetHeightOverride(30.f);
		if (Language.FlagCode)
		{
			auto* Texture = LoadObject<UTexture2D>(nullptr, *UGameLanguageSettingsSubsystem::GetFlagTexturePath(Language));
			if (!Texture)
			{
				UE_LOG(LogTemp, Error, TEXT("Settings layout: flag texture %s not found."), *UGameLanguageSettingsSubsystem::GetFlagTexturePath(Language));
				return false;
			}
			auto* Flag = Tree->ConstructWidget<UImage>(UImage::StaticClass(), *(Name + TEXT("_Flag")));
			Flag->SetBrushFromTexture(Texture, false);
			Flag->SetDesiredSizeOverride(FVector2D(40.f, 30.f));
			FlagBox->SetContent(Flag);
		}
		else
		{
			// 중국 출시 시 대만 국기 표기가 문제될 수 있어 국기 대신 국기 크기의 판에 "繁" 글자를 얹는다.
			auto* Badge = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(Name + TEXT("_Badge")));
			Badge->SetBrush(FactoryUI::Plate(FLinearColor(0.09f, 0.18f, 0.21f), FactoryUI::Cyan, 3.f));
			Badge->SetPadding(FMargin(0.f));
			Badge->SetHorizontalAlignment(HAlign_Center);
			Badge->SetVerticalAlignment(VAlign_Center);
			auto* Glyph = MakeSettingsLabel(Tree, FText::AsCultureInvariant(TEXT("繁")), 18);
			Glyph->Rename(*(Name + TEXT("_BadgeText")), Tree);
			UseNameFont(Glyph, 18);
			Glyph->SetRenderTranslation(FVector2D(0.f, -1.f));
			Badge->SetContent(Glyph);
			FlagBox->SetContent(Badge);
		}
		auto* FlagSlot = Row->AddChildToHorizontalBox(FlagBox);
		FlagSlot->SetVerticalAlignment(VAlign_Center);
		FlagSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));

		// 자국어 이름. 번역 대상이 아니므로 문화권 고정 텍스트로 넣는다(Localization 수집에서 빠진다).
		auto* Label = MakeSettingsLabel(Tree, FText::AsCultureInvariant(Language.NativeName), 20);
		Label->Rename(*(Name + TEXT("_Name")), Tree);
		UseNameFont(Label, 20);
		Label->SetRenderTranslation(FVector2D(0.f, -2.f));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		// 긴 이름(Español (Latinoamérica) 등)은 버튼 폭을 넘지 않도록 줄여서 보여 준다.
		auto* Fit = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), *(Name + TEXT("_Fit")));
		Fit->SetStretch(EStretch::ScaleToFit);
		Fit->SetStretchDirection(EStretchDirection::DownOnly);
		Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fit->SetContent(Label);
		auto* LabelSlot = Row->AddChildToHorizontalBox(Fit);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
		LabelSlot->SetVerticalAlignment(VAlign_Center);

		auto* GridSlot = Grid->AddChildToUniformGrid(Cell, Index / 2, Index % 2);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UE_LOG(LogTemp, Display, TEXT("Settings layout: language page with %d languages."), Languages.Num());
	return true;
}
#endif

void UAudioSettingsWidget::UpdateSettingsTabs(int32 ActiveIndex)
{
	for (const FSettingsTab& Tab : SettingsTabs)
	{
		if (auto* Label = Cast<UTextBlock>(FindSettingsWidget(Tab.Text)))
			Label->SetColorAndOpacity(Tab.Page == ActiveIndex ? FactoryUI::Cyan : FactoryUI::Text);
	}
}

void UAudioSettingsWidget::ShowSettingsTab(int32 Index)
{
	if (auto* Pages = Cast<UWidgetSwitcher>(FindSettingsWidget(TEXT("SettingsTabPages"))))
	{
		Pages->SetActiveWidgetIndex(Index);
		UpdateSettingsTabs(Index);
		if (Index == AppearancePage) RefreshAppearancePreview();
		if (Index == KeysPage) RefreshKeySelectors();
		FocusFirstControl();
	}
}

void UAudioSettingsWidget::ShowGeneralSettings() { ShowSettingsTab(GraphicsPage); }
void UAudioSettingsWidget::ShowAudioSettings() { ShowSettingsTab(AudioPage); }
void UAudioSettingsWidget::ShowAppearanceSettings() { ShowSettingsTab(AppearancePage); }
void UAudioSettingsWidget::ShowKeySettings() { ShowSettingsTab(KeysPage); }
void UAudioSettingsWidget::ShowLanguageSettings() { ShowSettingsTab(LanguagePage); }

void ULanguageButtonHandler::HandleClicked()
{
	if (UAudioSettingsWidget* Settings = Owner.Get()) Settings->SelectLanguage(Culture);
}

void UAudioSettingsWidget::BindLanguageButtons()
{
	// 화면을 다시 열어 NativeConstruct가 또 불려도 클릭이 두 번 처리되지 않도록 처음 한 번만 연결한다.
	if (LanguageButtons.IsEmpty())
	{
		for (const FGameLanguage& Language : UGameLanguageSettingsSubsystem::GetSupportedLanguages())
		{
			const FName Name = UGameLanguageSettingsSubsystem::GetLanguageWidgetName(Language);
			auto* Button = Cast<UButton>(FindSettingsWidget(Name));
			if (!Button) continue;
			auto* Handler = NewObject<ULanguageButtonHandler>(this);
			Handler->Culture = Language.Culture;
			Handler->Owner = this;
			Button->OnClicked.AddUniqueDynamic(Handler, &ULanguageButtonHandler::HandleClicked);
			FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Confirm);
			LanguageHandlers.Add(Handler);
			LanguageButtons.Add({Button, Name, Language.Culture, Button->GetStyle().Disabled});
		}
	}
	RefreshLanguageButtons();
}

void UAudioSettingsWidget::RefreshLanguageButtons()
{
	// 현재 언어는 탭 선택색과 같은 청록 테두리와 글자로 표시한다.
	// 메뉴 포커스 처리(FactoryMenuStyle::UpdateFocus)가 포커스가 없을 때 Disabled 브러시를 평소 모습으로 쓰므로 Disabled에 넣는다.
	const FString Current = UGameLanguageSettingsSubsystem::GetCurrentLanguage();
	for (const FLanguageButton& Entry : LanguageButtons)
	{
		UButton* Button = Entry.Button.Get();
		if (!Button) continue;
		const bool bCurrent = Entry.Culture == Current;
		FSlateBrush Brush = Entry.BaseBrush;
		if (bCurrent)
		{
			Brush.OutlineSettings.Color = FactoryUI::Cyan;
			Brush.OutlineSettings.Width = FMath::Max(Brush.OutlineSettings.Width, 2.f);
		}
		FButtonStyle Style = Button->GetStyle();
		Style.Disabled = Brush;
		if (!Button->HasAnyUserFocus()) Style.Normal = Brush;
		Button->SetStyle(Style);
		if (auto* Label = Cast<UTextBlock>(FindSettingsWidget(*(Entry.Name.ToString() + TEXT("_Name")))))
			Label->SetColorAndOpacity(bCurrent ? FactoryUI::Cyan : FactoryUI::Text);
	}
}

void UAudioSettingsWidget::SelectLanguage(const FString& Culture)
{
	// 고르는 즉시 적용하고 저장한다. FText로 만든 문구는 엔진이 바로 바꾸고, 드롭다운은 HandleCultureChanged가 다시 채운다.
	if (UGameLanguageSettingsSubsystem::GetCurrentLanguage() == Culture) return;
	if (!UGameLanguageSettingsSubsystem::SetLanguage(Culture))
		UE_LOG(LogTemp, Warning, TEXT("Language: could not switch to %s."), *Culture);
	RefreshLanguageButtons();
}

void UAudioSettingsWidget::BindSettingsHelp()
{
	auto* Text = Cast<UTextBlock>(FindSettingsWidget(TEXT("SettingsHelpText")));
	if (!Text || SettingsHelpText == Text) return;
	SettingsHelpText = Text;
	SettingsHelpEntries.Reset();

	const auto AddHelp = [&](UWidget* Control, int32 PageIndex, const FText& Fallback = FText::GetEmpty(), bool bWholeRow = true)
	{
		if (!Control) return;
		FText Help = Control->GetToolTipText();
		if (Help.IsEmpty()) Help = Fallback;
		if (Help.IsEmpty()) return;
		Control->SetToolTipText(FText::GetEmpty());
		UWidget* Row = Control;
		while (bWholeRow && Row->GetParent() && !Row->GetParent()->IsA<UVerticalBox>()) Row = Row->GetParent();
		SettingsHelpEntries.Add({Row, MoveTemp(Help), PageIndex});
	};
	for (const FName Name : DisplayHelpControls)
		AddHelp(FindSettingsWidget(Name), GraphicsPage);
	for (const FName Name : AppearanceHelpControls)
		AddHelp(FindSettingsWidget(Name), AppearancePage, FText::GetEmpty(), Name != TEXT("AppearancePreviewBox"));
	// 볼륨 줄의 도움말은 소리 페이지(AudioSettingsSection)의 툴팁 하나를 함께 쓴다.
	UWidget* Audio = FindSettingsWidget(TEXT("AudioSettingsSection"));
	const FText AudioHelp = Audio ? Audio->GetToolTipText() : FText::GetEmpty();
	if (Audio) Audio->SetToolTipText(FText::GetEmpty());
	if (const auto* VolumeLabel = Cast<UTextBlock>(FindSettingsWidget(TEXT("MasterVolumeSliderLabel"))))
		Text->SetFont(VolumeLabel->GetFont());
	for (const auto& Entry : VolumeRows)
		AddHelp(FindSettingsWidget(Entry.Slider), AudioPage, AudioHelp);
	for (const auto& Entry : KeySelectors)
		AddHelp(Entry.Key, KeysPage);
	AddHelp(FindSettingsWidget(TEXT("ResetGameplayKeys")), KeysPage);
	for (const FGameLanguage& Language : UGameLanguageSettingsSubsystem::GetSupportedLanguages())
		AddHelp(FindSettingsWidget(UGameLanguageSettingsSubsystem::GetLanguageWidgetName(Language)), LanguagePage,
			GetLanguageHelpText(), false);
}

void UAudioSettingsWidget::RefreshSettingsHelp()
{
	auto* Text = SettingsHelpText.Get();
	const auto* Pages = Text ? Cast<UWidgetSwitcher>(FindSettingsWidget(TEXT("SettingsTabPages"))) : nullptr;
	if (!Pages || !Text->GetParent() || !FSlateApplication::IsInitialized()) return;

	const int32 PageIndex = Pages->GetActiveWidgetIndex();
	const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
	const auto* Entry = SettingsHelpEntries.FindByPredicate([&](const FSettingsHelpEntry& Help)
	{
		const auto* Row = Help.Row.Get();
		return Help.PageIndex == PageIndex && Row && Row->GetCachedGeometry().IsUnderLocation(CursorPosition);
	});
	if (Entry)
	{
		FText HelpText = Entry->Text;
		if (PageIndex == AppearancePage)
		{
			for (const TCHAR* Name : {TEXT("OutlineColorSwatch"), TEXT("PlayerColorSwatch")})
			{
				const auto* Swatch = Cast<UTextBlock>(FindSettingsWidget(Name));
				if (!Swatch || !Swatch->GetCachedGeometry().IsUnderLocation(CursorPosition)) continue;
				const FString Hex = Swatch->GetColorAndOpacity().GetSpecifiedColor().ToFColorSRGB().ToHex().Left(6);
				HelpText = FText::Format(NSLOCTEXT("SettingsHelp", "CurrentColor", "현재 색상 #{0}"), FText::FromString(Hex));
				break;
			}
		}
		if (!Text->GetText().EqualTo(HelpText)) Text->SetText(HelpText);
	}
	const ESlateVisibility HelpVisibility = Entry ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (Text->GetParent()->GetVisibility() != HelpVisibility) Text->GetParent()->SetVisibility(HelpVisibility);
}

#if WITH_EDITOR
void UAudioSettingsWidget::AuthorKeySettingsPage(UWidgetTree* WidgetTree, UWidgetSwitcher* Pages)
{
	auto* Page = WidgetTree->ConstructWidget<UVerticalBox>();
	Pages->AddChild(Page);
	auto Bindings = UGameKeySettingsSubsystem::GetDefaultBindings();
	const auto IsJump = [](const FGameKeyBinding& Binding) { return Binding.WidgetName.ToString().StartsWith(TEXT("IA_Jump_")); };
	const auto IsDrop = [](const FGameKeyBinding& Binding) { return Binding.WidgetName.ToString().StartsWith(TEXT("IA_Drop_")); };
	const int32 DropIndex = Bindings.IndexOfByPredicate(IsDrop);
	if (DropIndex != INDEX_NONE && Bindings.ContainsByPredicate(IsJump))
	{
		const auto Drop = Bindings[DropIndex];
		Bindings.RemoveAt(DropIndex);
		Bindings.Insert(Drop, Bindings.IndexOfByPredicate(IsJump) + 1);
	}
	if (Bindings.IsEmpty())
	{
		Page->AddChildToVerticalBox(MakeSettingsLabel(WidgetTree,
			NSLOCTEXT("KeySettings", "Unavailable", "키 설정을 불러올 수 없습니다."), 19));
		return;
	}

	auto* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Page->AddChildToVerticalBox(Columns)->SetPadding(FMargin(4.f, 0.f));
	UVerticalBox* BindingColumns[2];
	for (int32 Index = 0; Index < 2; ++Index)
	{
		BindingColumns[Index] = WidgetTree->ConstructWidget<UVerticalBox>();
		auto* ColumnSlot = Columns->AddChildToHorizontalBox(BindingColumns[Index]);
		ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ColumnSlot->SetPadding(FMargin(0.f, 0.f, Index == 0 ? 24.f : 0.f, 0.f));
	}
	auto TextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
	TextStyle.SetColorAndOpacity(FactoryUI::Text);
	FButtonStyle KeyButtonStyle = FactoryUI::Button();
	KeyButtonStyle.NormalPadding = FMargin(6.f, 0.f);
	KeyButtonStyle.PressedPadding = KeyButtonStyle.NormalPadding;
	const int32 LeftCount = (Bindings.Num() + 1) / 2;
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const auto& Binding = Bindings[Index];
		auto* Selector = WidgetTree->ConstructWidget<UInputKeySelector>(UInputKeySelector::StaticClass(), Binding.WidgetName);
		Selector->SetButtonStyle(KeyButtonStyle);
		Selector->SetTextStyle(TextStyle);
		Selector->SetAllowModifierKeys(false);
		Selector->SetAllowGamepadKeys(false);
		Selector->SetKeySelectionText(NSLOCTEXT("KeySettings", "Capture", "키 입력…"));
		Selector->SetToolTipText(NSLOCTEXT("KeySettings", "CaptureTip", "클릭 후 새 키를 누르세요. Esc로 취소할 수 있습니다."));
		UWidget* Control = Selector;
		if (IsDrop(Binding))
		{
			auto* Chord = WidgetTree->ConstructWidget<UHorizontalBox>();
			auto* DownKeySlot = Chord->AddChildToHorizontalBox(Selector);
			DownKeySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			auto* JumpKey = MakeSettingsLabel(WidgetTree, FText::GetEmpty(), 17);
			JumpKey->Rename(TEXT("DropJumpKey"), WidgetTree);
			JumpKey->SetRenderTranslation(FVector2D(0.f, -3.f));
			auto* JumpKeySlot = Chord->AddChildToHorizontalBox(JumpKey);
			JumpKeySlot->SetVerticalAlignment(VAlign_Center);
			JumpKeySlot->SetPadding(FMargin(8.f, 0.f));
			Control = Chord;
		}
		auto* Column = BindingColumns[Index < LeftCount ? 0 : 1];
		AddSettingsRow(WidgetTree, Column, Binding.Label, Control);
		CastChecked<USizeBox>(Column->GetChildAt(Column->GetChildrenCount() - 1))->SetHeightOverride(42.f);
	}

	auto* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
	Page->AddChildToVerticalBox(Footer)->SetPadding(FMargin(4.f, 6.f, 4.f, 0.f));
	auto* Status = MakeSettingsLabel(WidgetTree, FText::GetEmpty(), 18);
	Status->Rename(TEXT("KeySettingsStatus"), WidgetTree);
	Status->SetColorAndOpacity(FactoryUI::Amber);
	auto* StatusSlot = Footer->AddChildToHorizontalBox(Status);
	StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StatusSlot->SetVerticalAlignment(VAlign_Center);
	auto* ResetRow = WidgetTree->ConstructWidget<UVerticalBox>();
	Footer->AddChildToHorizontalBox(ResetRow);
	FactoryMenuStyle::AddButton(WidgetTree, ResetRow, TEXT("ResetGameplayKeys"), TEXT("ResetGameplayKeysText"),
		NSLOCTEXT("KeySettings", "Defaults", "기본값 복원"), true);
	auto* Reset = CastChecked<UButton>(WidgetTree->FindWidget(TEXT("ResetGameplayKeys")));
	auto* ResetBox = CastChecked<USizeBox>(Reset->GetParent());
	ResetBox->SetHeightOverride(42.f);
	ResetBox->SetWidthOverride(190.f);
	auto* ResetLabel = CastChecked<UTextBlock>(WidgetTree->FindWidget(TEXT("ResetGameplayKeysText")));
	ResetLabel->SetAutoWrapText(false);
	auto* ResetSlot = CastChecked<UButtonSlot>(ResetLabel->Slot);
	ResetSlot->SetVerticalAlignment(VAlign_Center);
	ResetSlot->SetPadding(FMargin(10.f, 0.f));
}
#endif

void UAudioSettingsWidget::BindKeySelectors()
{
	KeySelectors.Reset();
	FirstKeySelector.Reset();
	if (const auto* Settings = ULocalPlayer::GetSubsystem<UGameKeySettingsSubsystem>(GetOwningLocalPlayer()))
	{
		for (const auto& Binding : Settings->GetBindings())
		{
			if (auto* Selector = Cast<UInputKeySelector>(FindSettingsWidget(Binding.WidgetName)))
			{
				KeySelectors.Add(Selector, Binding.MappingIndex);
				if (Binding.WidgetName.ToString().StartsWith(TEXT("IA_Jump_")) || !FirstKeySelector.IsValid())
					FirstKeySelector = Selector;
				Selector->OnIsSelectingKeyChanged.AddUniqueDynamic(this, &ThisClass::OnKeySelectionModeChanged);
				Selector->OnKeySelected.AddUniqueDynamic(this, &ThisClass::OnGameplayKeySelected);
			}
		}
	}
	if (auto* Reset = Cast<UButton>(FindSettingsWidget(TEXT("ResetGameplayKeys"))))
	{
		Reset->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetGameplayKeys);
		FactoryUI::ApplyButtonSounds(Reset, FactoryUI::ESound::Confirm);
	}
	RefreshKeySelectors();
	UpdateSettingsTabs(0);
}

void UAudioSettingsWidget::OnKeySelectionModeChanged()
{
	for (const auto& Entry : KeySelectors)
	{
		if (Entry.Key->GetIsSelectingKey())
		{
			CapturingKey = Entry.Key;
			if (auto* Status = Cast<UTextBlock>(FindSettingsWidget(TEXT("KeySettingsStatus"))))
				Status->SetText(FText::GetEmpty());
			break;
		}
	}
}

void UAudioSettingsWidget::OnGameplayKeySelected(FInputChord Chord)
{
	if (bRefreshingKeys) return;
	const int32* Index = KeySelectors.Find(CapturingKey.Get());
	CapturingKey.Reset();
	auto* Settings = ULocalPlayer::GetSubsystem<UGameKeySettingsSubsystem>(GetOwningLocalPlayer());
	if (!Index || !Settings) return;
	FText Error;
	Settings->TrySetKey(*Index, Chord.Key, Error);
	if (auto* Status = Cast<UTextBlock>(FindSettingsWidget(TEXT("KeySettingsStatus"))))
		Status->SetText(Error);
	RefreshKeySelectors();
}

void UAudioSettingsWidget::RefreshKeySelectors()
{
	const auto* Settings = ULocalPlayer::GetSubsystem<UGameKeySettingsSubsystem>(GetOwningLocalPlayer());
	if (!Settings) return;
	const auto Bindings = Settings->GetBindings();
	TGuardValue<bool> Guard(bRefreshingKeys, true);
	for (const auto& Entry : KeySelectors)
	{
		const auto* Binding = Bindings.FindByPredicate([&Entry](const FGameKeyBinding& Row) { return Row.MappingIndex == Entry.Value; });
		if (Binding) Entry.Key->SetSelectedKey(FInputChord(Binding->Key));
	}
}

void UAudioSettingsWidget::ResetGameplayKeys()
{
	if (auto* Settings = ULocalPlayer::GetSubsystem<UGameKeySettingsSubsystem>(GetOwningLocalPlayer()))
		Settings->ResetKeys();
	if (auto* Status = Cast<UTextBlock>(FindSettingsWidget(TEXT("KeySettingsStatus"))))
		Status->SetText(FText::GetEmpty());
	RefreshKeySelectors();
}

void UAudioSettingsWidget::ResetAppearanceSettings()
{
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Visual) return;
	Visual->ResetAppearance();
	{
		TGuardValue<bool> Guard(bSynchronizingAppearanceControls, true);
		for (const TCHAR* Name : {TEXT("OutlineColorSetting"), TEXT("PlayerColorSetting")})
			if (auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(Name))) Combo->SetSelectedIndex(0);
		if (auto* Slider = Cast<USlider>(FindSettingsWidget(TEXT("OutlineWidthSetting")))) Slider->SetValue(Visual->OutlineScale);
		if (auto* Slider = Cast<USlider>(FindSettingsWidget(TEXT("OutlineBrightnessSetting")))) Slider->SetValue(Visual->OutlineBrightness);
	}
	UpdateVisuals();
}

void UAudioSettingsWidget::RefreshAppearancePreview()
{
	auto* Box = Cast<USizeBox>(FindSettingsWidget(TEXT("AppearancePreviewBox")));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Box || !Visual || !FApp::CanEverRender()) return;

	auto* Mesh = AppearancePreviewMesh.Get();
	if (!Mesh)
	{
		const UClass* PlayerClass = LoadClass<APlayerBase>(nullptr,
			TEXT("/Game/Main/Blueprints/Character/Player/BP_MainPlayer.BP_MainPlayer_C"));
		const auto* Player = PlayerClass ? PlayerClass->GetDefaultObject<APlayerBase>() : nullptr;
		const auto* Template = Player ? Player->GetMesh() : nullptr;
		if (!Template || !Template->GetSkeletalMeshAsset())
		{
			Box->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		auto* Viewport = Cast<UViewport>(Box->GetContent());
		if (!Viewport) return;
		auto* Actor = Cast<ASkeletalMeshActor>(Viewport->Spawn(ASkeletalMeshActor::StaticClass()));
		if (!Actor) return;
		Mesh = Actor->GetSkeletalMeshComponent();
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetSkeletalMesh(Template->GetSkeletalMeshAsset());
		Mesh->SetRelativeTransform(Template->GetRelativeTransform());
		for (int32 Index = 0; Index < Template->GetNumMaterials(); ++Index)
			Mesh->SetMaterial(Index, Template->GetMaterial(Index));
		Mesh->SetOverlayMaterial(Template->GetOverlayMaterial());
		AppearancePreviewBody = Template->GetMaterial(0);
		AppearancePreviewMesh = Mesh;

		const FBoxSphereBounds Bounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
		const FVector Direction = FVector(0.72, 0.69, 0.1).GetSafeNormal();
		const double Distance = FMath::Max(1.65 * Bounds.BoxExtent.Z, 1.4 * Bounds.SphereRadius);
		const FRotator ViewRotation = (-Direction).Rotation();
		const FTransform MeshToWorld = Mesh->GetComponentTransform();
		const FBox MeshBox = Mesh->GetSkeletalMeshAsset()->GetImportedBounds().GetBox();
		FVector ViewLocation = MeshToWorld.TransformPosition(MeshBox.GetCenter()) + Distance * Direction;
		TArray<FVector> ViewPoints;
		const FSkeletalMeshRenderData* RenderData = Mesh->GetSkeletalMeshRenderData();
		if (RenderData && RenderData->LODRenderData.Num() > 0)
		{
			const auto& Positions = RenderData->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
			if (Positions.GetVertexData())
			{
				ViewPoints.Reserve(Positions.GetNumVertices());
				for (uint32 Index = 0; Index < Positions.GetNumVertices(); ++Index)
					ViewPoints.Add(FVector(Positions.VertexPosition(Index)));
			}
		}
		if (ViewPoints.IsEmpty())
		{
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				ViewPoints.Emplace(
					(Corner & 1) ? MeshBox.Max.X : MeshBox.Min.X,
					(Corner & 2) ? MeshBox.Max.Y : MeshBox.Min.Y,
					(Corner & 4) ? MeshBox.Max.Z : MeshBox.Min.Z);
			}
		}
		for (FVector& Point : ViewPoints)
			Point = ViewRotation.UnrotateVector(MeshToWorld.TransformPosition(Point) - ViewLocation);

		// Use the projected geometry so empty corners of the mesh bounds do not bias the framing.
		FVector2D CameraOffset = FVector2D::ZeroVector;
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			FBox2D ProjectedBounds(ForceInit);
			for (const FVector& Point : ViewPoints)
				if (Point.X > UE_SMALL_NUMBER)
					ProjectedBounds += (FVector2D(Point.Y, Point.Z) - CameraOffset) / Point.X;
			if (!ProjectedBounds.bIsValid) break;
			CameraOffset += ProjectedBounds.GetCenter() * Distance;
		}
		const double HorizontalOffset = CameraOffset.X + Distance * AppearancePreviewHorizontalOffset;
		ViewLocation += ViewRotation.RotateVector(FVector(0.0, HorizontalOffset, CameraOffset.Y));
		Viewport->SetViewLocation(ViewLocation);
		Viewport->SetViewRotation(ViewRotation);
		AddPreviewBackdrop(Actor, ViewLocation, ViewRotation, Distance);
		Viewport->SetLightIntensity(2.0f);
		Viewport->SetSkyIntensity(1.2f);
	}
	Visual->ApplyPreviewAppearance(Mesh, AppearancePreviewBody);
}

void UAudioSettingsWidget::BuildVisualControls()
{
	auto* Column = Cast<UVerticalBox>(FindSettingsWidget(TEXT("VisualSettingsColumn")));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Column || !Visual || !Screen) return;
	Resolutions.Reset();

	auto AddSlider = [&](FName Name, float Value, float Maximum)
	{
		auto* Slider = Cast<USlider>(FindSettingsWidget(Name));
		if (!Slider) return Slider;
		StyleSettingsSlider(Slider);
		Slider->SetMaxValue(Maximum);
		Slider->SetValue(Value);
		Slider->SetStepSize(VisualSliderStep);
		Slider->RequiresControllerLock = false;
		return Slider;
	};
	auto AddCombo = [&](FName Name, const TArray<FString>& Options, int32 Selected)
	{
		auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(Name));
		if (!Combo) return Combo;
		Combo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::MakeDropdownOption);
		Combo->ClearOptions();
		for (const FString& Option : Options) Combo->AddOption(Option);
		Combo->SetSelectedIndex(Selected);
		return Combo;
	};
	auto AddToggle = [&](FName Name, bool bValue)
	{
		auto* Check = Cast<UCheckBox>(FindSettingsWidget(Name));
		if (Check)
		{
			StyleSettingsCheckBox(Check);
			Check->SetIsChecked(bValue);
		}
		return Check;
	};
	// 디자이너에서 위젯 이름을 바꾸거나 지워도 패키징 빌드에서 멈추지 않도록 모두 null을 확인한다.
	if (auto* BrightnessSlider = AddSlider(TEXT("BrightnessSetting"), Visual->Brightness, 1.f))
	{
		if (!FindSettingsWidget(TEXT("SettingsHelpText")) && BrightnessSlider->GetToolTipText().IsEmpty())
			BrightnessSlider->SetToolTipText(BrightnessHelpText);
		BrightnessSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetBrightness);
	}
	if (auto* Mode = AddCombo(TEXT("WindowModeSetting"),
		ToOptionStrings(GetWindowModeOptions()), static_cast<int32>(Screen->GetFullscreenMode())))
		Mode->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetWindowMode);
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	const FIntPoint Current = Screen->GetScreenResolution();
	Resolutions.AddUnique(Current);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	TArray<FString> ResolutionLabels;
	for (const FIntPoint& Resolution : Resolutions)
		ResolutionLabels.Add(FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y));
	if (auto* ResolutionCombo = AddCombo(TEXT("ResolutionSetting"), ResolutionLabels, Resolutions.IndexOfByKey(Current)))
	{
		ResolutionCombo->SetIsEnabled(Screen->GetFullscreenMode() != EWindowMode::WindowedFullscreen);
		ResolutionCombo->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnResolutionChanged);
	}
	if (auto* VSync = AddToggle(TEXT("VSyncSetting"), Screen->IsVSyncEnabled()))
		VSync->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::SetVSync);
	const int32 Limit = FMath::RoundToInt(Screen->GetFrameRateLimit());
	int32 LimitIndex = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameLimitValues); ++Index)
		if (FrameLimitValues[Index] == Limit) LimitIndex = Index;
	if (auto* FrameLimit = AddCombo(TEXT("FrameLimitSetting"), ToOptionStrings(GetFrameLimitOptions()), LimitIndex))
		FrameLimit->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetFrameLimit);
	// 엔진 단계 4(시네마틱)나 그룹별로 섞인 값(-1)은 메뉴에 없으므로 "사용자 지정"으로 보여 준다.
	const int32 QualityLevel = GetQualityPresetLevel(*Screen);
	TArray<FString> QualityOptions = ToOptionStrings(GetQualityPresetOptions());
	const bool bPresetLevel = QualityLevel >= 0 && QualityLevel < QualityPresetCount;
	if (!bPresetLevel) QualityOptions.Add(GetCustomQualityOption().ToString());
	if (auto* Quality = AddCombo(TEXT("QualityPresetSetting"), QualityOptions, bPresetLevel ? QualityLevel : QualityPresetCount))
		Quality->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetQualityPreset);
	if (auto* RenderScale = Cast<USlider>(FindSettingsWidget(TEXT("RenderScaleSetting"))))
	{
		StyleSettingsSlider(RenderScale);
		RenderScale->SetMinValue(FMath::Clamp(MinRenderScalePercent, 10.f, 100.f) / 100.f);
		RenderScale->SetMaxValue(1.f);
		RenderScale->SetStepSize(RenderScaleStepPercent / 100.f);
		RenderScale->SetValue(GetRenderScalePercent(*Screen) / 100.f);
		RenderScale->MouseUsesStep = true;
		RenderScale->RequiresControllerLock = false;
		RenderScale->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetRenderResolutionScale);
	}
	if (auto* ColorVision = AddCombo(TEXT("ColorVisionSetting"), ToOptionStrings(GetColorVisionOptions()), Visual->ColorVision))
		ColorVision->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetColorVision);
	if (auto* Strength = AddSlider(TEXT("ColorVisionStrengthSetting"), Visual->ColorVisionStrength, 1.f))
	{
		Strength->SetIsEnabled(Visual->ColorVision != 0);
		Strength->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetColorVisionStrength);
	}
	if (auto* Shake = AddSlider(TEXT("ShakeSetting"), Visual->ShakeScale, 1.f))
		Shake->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetShake);
	if (auto* Bloom = AddToggle(TEXT("BloomSetting"), Visual->bBloom))
		Bloom->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::SetBloom);
	if (auto* ReduceFlashes = AddToggle(TEXT("ReduceFlashesSetting"), Visual->bReduceFlashes))
		ReduceFlashes->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::SetReduceFlashes);
	const TArray<FString> AppearanceColors = ToOptionStrings(GetAppearanceColorOptions());
	static_assert(UGameVisualSettingsSubsystem::CustomColorIndex == 6, "외형 색상 항목(GetAppearanceColorOptions)은 사용자 지정까지 7개다.");
	if (auto* OutlineColor = AddCombo(TEXT("OutlineColorSetting"), AppearanceColors, Visual->OutlineColor))
		OutlineColor->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetOutlineColor);
	if (auto* PlayerColor = AddCombo(TEXT("PlayerColorSetting"), AppearanceColors, Visual->PlayerColor))
		PlayerColor->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetPlayerColor);
	if (auto* OutlineWidth = AddSlider(TEXT("OutlineWidthSetting"), Visual->OutlineScale, 2.f))
		OutlineWidth->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetOutlineWidth);
	if (auto* OutlineBrightness = AddSlider(TEXT("OutlineBrightnessSetting"), Visual->OutlineBrightness, 2.f))
		OutlineBrightness->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetOutlineBrightness);
	if (auto* Apply = Cast<UButton>(FindSettingsWidget(TEXT("ApplyScreenButton"))))
		Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplyScreenMode);
	RefreshVisualReadouts();
	RefreshApplyButton();
}

void UAudioSettingsWidget::HandleCultureChanged()
{
	RefreshLanguageButtons();
	// 화면 모드 확인 중에는 드롭다운이 잠겨 있고 선택 값이 아직 확정되지 않았으므로 다시 채우지 않는다.
	if (ScreenConfirmation.IsValid()) return;
	// 아직 적용하지 않은 화면 모드와 해상도 선택은 그대로 둔다.
	TMap<FName, int32> PendingSelections;
	for (const FName Name : {FName(TEXT("WindowModeSetting")), FName(TEXT("ResolutionSetting"))})
		if (const auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(Name))) PendingSelections.Add(Name, Combo->GetSelectedIndex());
	BuildVisualControls();
	for (const auto& Pending : PendingSelections)
		if (auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(Pending.Key)); Combo && Pending.Value >= 0)
			Combo->SetSelectedIndex(Pending.Value);
	RefreshApplyButton();
}

UWidget* UAudioSettingsWidget::MakeDropdownOption(FString Option)
{
	// 글꼴과 색은 그 콤보박스의 디자이너 값(Font, Foreground Color, Item Style의 Text Color)을 그대로 쓴다.
	// 코드는 F_TLM_UI의 글자가 줄 높이보다 아래로 그려지는 것만 보정한다(라벨의 -5 오프셋과 같은 이유).
	const UComboBoxString* Source = nullptr;
	for (const TCHAR* Name : {TEXT("WindowModeSetting"), TEXT("ResolutionSetting"), TEXT("FrameLimitSetting"),
		TEXT("QualityPresetSetting"), TEXT("ColorVisionSetting"), TEXT("OutlineColorSetting"), TEXT("PlayerColorSetting")})
		if (const auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(Name)); Combo && Combo->FindOptionIndex(Option) >= 0)
		{
			Source = Combo;
			break;
		}
	auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
	if (Source) Label->SetFont(Source->GetFont());
	Label->SetColorAndOpacity(FSlateColor::UseForeground());
	Label->SetText(FText::FromString(Option));
	Label->SetRenderTranslation(DropdownTextOffset);
	return Label;
}

void UAudioSettingsWidget::UpdateVisuals()
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance()))
	{
		Visual->ApplyDisplay();
		Visual->ApplyAppearance(Cast<APlayerBase>(GetOwningPlayerPawn()));
		if (AppearancePreviewMesh.IsValid()) RefreshAppearancePreview();
		RefreshVisualReadouts();
	}
}

void UAudioSettingsWidget::RefreshVisualReadouts()
{
	const auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Visual) return;
	FNumberFormattingOptions Decimal;
	Decimal.MinimumFractionalDigits = 2;
	Decimal.MaximumFractionalDigits = 2;
	struct FReadout { const TCHAR* Control; const TCHAR* Name; FText Text; };
	const auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const float RenderScale = Screen ? GetRenderScalePercent(*Screen) / 100.f : 1.f;
	const FReadout Entries[] = {
		{TEXT("RenderScaleSetting"), TEXT("RenderScaleValue"), FText::AsPercent(RenderScale)},
		{TEXT("ColorVisionStrengthSetting"), TEXT("ColorVisionStrengthValue"), FText::AsPercent(Visual->ColorVisionStrength)},
		{TEXT("BrightnessSetting"), TEXT("BrightnessValue"), FText::AsPercent(Visual->Brightness)},
		{TEXT("ShakeSetting"), TEXT("ShakeValue"), FText::AsPercent(Visual->ShakeScale)},
		{TEXT("OutlineWidthSetting"), TEXT("OutlineWidthValue"), FText::Format(
			NSLOCTEXT("Settings", "OutlineMultiplier", "{0}×"), FText::AsNumber(Visual->OutlineScale, &Decimal))},
		{TEXT("OutlineBrightnessSetting"), TEXT("OutlineBrightnessValue"), FText::AsPercent(Visual->OutlineBrightness)},
		{TEXT("OutlineColorSetting"), TEXT("OutlineColorSwatch"), FText::FromString(TEXT("■"))},
		{TEXT("PlayerColorSetting"), TEXT("PlayerColorSwatch"), FText::FromString(TEXT("■"))},
	};
	for (const FReadout& Entry : Entries)
	{
		auto* Label = Cast<UTextBlock>(FindSettingsWidget(Entry.Name));
		if (!Label)
		{
			auto* Control = FindSettingsWidget(Entry.Control);
			auto* Row = Control ? Cast<UHorizontalBox>(Control->GetParent()) : nullptr;
			// 디자이너에 수치 TextBlock이 없으면 만든다. 그 줄이 속한 페이지 WBP의 위젯 트리에 만든다.
			UWidgetTree* RowTree = Row ? Row->GetTypedOuter<UWidgetTree>() : nullptr;
			if (!RowTree) continue;
			Label = RowTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Entry.Name);
			auto Font = Label->GetFont();
			if (auto* Body = GetDefault<UFactoryUIAssetSettings>()->BodyFont.LoadSynchronous()) Font.FontObject = Body;
			Font.Size = ReadoutFontSize;
			Label->SetFont(Font);
			Label->SetMinDesiredWidth(ReadoutMinWidth);
			Label->SetJustification(ETextJustify::Right);
			Label->SetRenderTranslation(ReadoutOffset);
			Label->SetColorAndOpacity(ReadoutColor);
			UWidget* Readout = Label;
			if (FName(Entry.Name) == TEXT("OutlineWidthValue"))
			{
				auto* FixedWidth = RowTree->ConstructWidget<USizeBox>();
				FixedWidth->SetWidthOverride(FMath::Max(64.f, ReadoutMinWidth));
				FixedWidth->SetContent(Label);
				Readout = FixedWidth;
			}
			auto* ReadoutSlot = Row->AddChildToHorizontalBox(Readout);
			ReadoutSlot->SetPadding(ReadoutPadding);
			ReadoutSlot->SetVerticalAlignment(VAlign_Center);
		}
		Label->SetText(Entry.Text);
		const FName ReadoutName(Entry.Name);
		const bool bOutlineSwatch = ReadoutName == TEXT("OutlineColorSwatch");
		if (bOutlineSwatch || ReadoutName == TEXT("PlayerColorSwatch"))
		{
			const FLinearColor Color = bOutlineSwatch
				? Visual->GetOutlineColor(Cast<APlayerBase>(GetOwningPlayerPawn()))
				: Visual->GetPlayerColor();
			Label->SetColorAndOpacity(Color);
		}
	}
}

void UAudioSettingsWidget::SetBrightness(float Value)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->Brightness = Value;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetOutlineWidth(float Value)
{
	if (bSynchronizingAppearanceControls) return;
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->OutlineScale = Value;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetOutlineBrightness(float Value)
{
	if (bSynchronizingAppearanceControls) return;
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->OutlineBrightness = Value;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetShake(float Value)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->ShakeScale = Value;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetOutlineColor(FString Value, ESelectInfo::Type Selection)
{
	SetAppearanceColor(Value, Selection, true);
}

void UAudioSettingsWidget::SetPlayerColor(FString Value, ESelectInfo::Type Selection)
{
	SetAppearanceColor(Value, Selection, false);
}

void UAudioSettingsWidget::SetAppearanceColor(const FString& Value, ESelectInfo::Type Selection, bool bOutline)
{
	if (bSynchronizingAppearanceControls || Selection == ESelectInfo::Direct) return;
	const FName ComboName = bOutline ? TEXT("OutlineColorSetting") : TEXT("PlayerColorSetting");
	auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(ComboName));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Combo || !Visual) return;
	const int32 Index = Combo->FindOptionIndex(Value);
	if (Index < 0) return;
	int32& SelectedColor = bOutline ? Visual->OutlineColor : Visual->PlayerColor;
	if (Index == UGameVisualSettingsSubsystem::CustomColorIndex)
	{
		Combo->SetSelectedIndex(SelectedColor);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, bOutline](float)
		{
			OpenAppearancePalette(bOutline);
			return false;
		}));
		return;
	}
	if (SelectedColor == Index) return;
	SelectedColor = Index;
	UpdateVisuals();
}

void UAudioSettingsWidget::OpenAppearancePalette(bool bOutline)
{
	const auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Visual) return;
	const FLinearColor InitialColor = bOutline ? Visual->CustomOutlineColor : Visual->CustomPlayerColor;
	FColorPickerArgs Args(InitialColor,
		FOnLinearColorValueChanged::CreateWeakLambda(this, [this, bOutline](FLinearColor Color)
		{
			auto* Settings = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
			if (!Settings) return;
			(bOutline ? Settings->CustomOutlineColor : Settings->CustomPlayerColor) = Color.GetClamped();
			(bOutline ? Settings->OutlineColor : Settings->PlayerColor) = UGameVisualSettingsSubsystem::CustomColorIndex;
			const FName ComboName = bOutline ? TEXT("OutlineColorSetting") : TEXT("PlayerColorSetting");
			if (auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(ComboName)))
				Combo->SetSelectedIndex(UGameVisualSettingsSubsystem::CustomColorIndex);
			UpdateVisuals();
		}));
	Args.ParentWidget = TakeWidget();
	Args.bIsModal = true;
	Args.bOnlyRefreshOnOk = true;
	Args.bClampValue = true;
	Args.OnColorPickerCancelled.BindLambda([](FLinearColor) {});
	OpenColorPicker(Args);
}

void UAudioSettingsWidget::SetBloom(bool bValue)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->bBloom = bValue;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetReduceFlashes(bool bValue)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance()))
	{
		Visual->bReduceFlashes = bValue;
		Visual->Save();
	}
}

void UAudioSettingsWidget::SetWindowMode(FString Value, ESelectInfo::Type Selection)
{
	auto* Mode = Cast<UComboBoxString>(FindSettingsWidget(TEXT("WindowModeSetting")));
	auto* Resolution = FindSettingsWidget(TEXT("ResolutionSetting"));
	if (Mode && Resolution) Resolution->SetIsEnabled(Mode->FindOptionIndex(Value) != static_cast<int32>(EWindowMode::WindowedFullscreen));
	RefreshApplyButton();
}

void UAudioSettingsWidget::OnResolutionChanged(FString Value, ESelectInfo::Type Selection) { RefreshApplyButton(); }

void UAudioSettingsWidget::RefreshApplyButton()
{
	auto* Apply = Cast<UButton>(FindSettingsWidget(TEXT("ApplyScreenButton")));
	auto* Mode = Cast<UComboBoxString>(FindSettingsWidget(TEXT("WindowModeSetting")));
	auto* Resolution = Cast<UComboBoxString>(FindSettingsWidget(TEXT("ResolutionSetting")));
	const auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Apply || !Mode || !Resolution || !Screen) return;
	const int32 Index = Resolution->GetSelectedIndex();
	const int32 ModeIndex = Mode->GetSelectedIndex();
	const bool bChanged = ModeIndex != static_cast<int32>(Screen->GetFullscreenMode())
		|| (ModeIndex != EWindowMode::WindowedFullscreen && Resolutions.IsValidIndex(Index)
			&& Resolutions[Index] != Screen->GetScreenResolution());
	Apply->SetIsEnabled(ScreenConfirmation.IsValid() || (ModeIndex >= 0 && bChanged));
}

void UAudioSettingsWidget::SetFrameLimit(FString Value, ESelectInfo::Type Selection)
{
	// 표시 문구는 언어마다 달라서 항목 순서로 값을 정한다. 다시 채울 때(Direct)는 값이 바뀌지 않았으므로 무시한다.
	auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(TEXT("FrameLimitSetting")));
	const int32 Index = Combo ? Combo->FindOptionIndex(Value) : INDEX_NONE;
	if (Selection == ESelectInfo::Direct || Index < 0 || Index >= UE_ARRAY_COUNT(FrameLimitValues)) return;
	if (auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Screen->SetFrameRateLimit(static_cast<float>(FrameLimitValues[Index]));
		Screen->ApplyNonResolutionSettings();
		if (!ScreenConfirmation.IsValid()) Screen->SaveSettings();
	}
}

void UAudioSettingsWidget::SetVSync(bool bValue)
{
	if (auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Screen->SetVSyncEnabled(bValue);
		Screen->ApplyNonResolutionSettings();
		if (!ScreenConfirmation.IsValid()) Screen->SaveSettings();
	}
}

int32 UAudioSettingsWidget::GetQualityPresetLevel(const UGameUserSettings& Screen)
{
	// 엔진의 GetOverallScalabilityLevel은 렌더링 해상도까지 단계 기본값과 같아야 단계를 돌려준다.
	// 이 메뉴는 품질을 바꿔도 사용자가 고른 해상도를 유지하므로, 해상도를 빼고 판정한다.
	const Scalability::FQualityLevels& Q = Screen.ScalabilityQuality;
	const int32 Level = Q.ViewDistanceQuality;
	const int32 Groups[] = {Q.AntiAliasingQuality, Q.ShadowQuality, Q.GlobalIlluminationQuality, Q.ReflectionQuality,
		Q.PostProcessQuality, Q.TextureQuality, Q.EffectsQuality, Q.FoliageQuality, Q.ShadingQuality, Q.LandscapeQuality};
	for (const int32 Group : Groups)
		if (Group != Level) return -1;
	return Level;
}

void UAudioSettingsWidget::SetQualityPreset(FString Value, ESelectInfo::Type Selection)
{
	if (Selection == ESelectInfo::Direct) return;
	auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(TEXT("QualityPresetSetting")));
	auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Combo || !Screen) return;
	const int32 Level = Combo->FindOptionIndex(Value);
	// "사용자 지정"은 현재 상태를 보여 주는 항목일 뿐 고를 수 있는 값이 아니다.
	if (Level < 0 || Level >= QualityPresetCount) return;
	// 엔진의 전체 품질 단계에는 렌더링 해상도도 들어 있어서, 품질을 바꿔도 사용자가 고른 비율이 유지되도록 되돌려 놓는다.
	const float RenderScalePercent = GetRenderScalePercent(*Screen);
	Screen->SetOverallScalabilityLevel(Level);
	Screen->SetResolutionScaleValueEx(RenderScalePercent);
	Screen->ApplyNonResolutionSettings();
	if (!ScreenConfirmation.IsValid()) Screen->SaveSettings();
	if (Combo->GetOptionCount() > QualityPresetCount) Combo->RemoveOption(Combo->GetOptionAtIndex(QualityPresetCount));
	// 빛 번짐 같은 개별 설정이 품질 단계 값에 덮이지 않도록 다시 적용한다.
	UpdateVisuals();
}

void UAudioSettingsWidget::SetRenderResolutionScale(float Value)
{
	auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Screen) return;
	const float Step = FMath::Max(RenderScaleStepPercent, 1.f);
	const float Percent = FMath::Clamp(FMath::GridSnap(Value * 100.f, Step), FMath::Clamp(MinRenderScalePercent, 10.f, 100.f), 100.f);
	if (FMath::IsNearlyEqual(Percent, GetRenderScalePercent(*Screen))) return;
	Screen->SetResolutionScaleValueEx(Percent);
	Screen->ApplyNonResolutionSettings();
	RefreshVisualReadouts();
}

void UAudioSettingsWidget::SetColorVision(FString Value, ESelectInfo::Type Selection)
{
	if (Selection == ESelectInfo::Direct) return;
	auto* Combo = Cast<UComboBoxString>(FindSettingsWidget(TEXT("ColorVisionSetting")));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Combo || !Visual) return;
	const int32 Index = Combo->FindOptionIndex(Value);
	if (Index < 0 || Index >= UGameVisualSettingsSubsystem::ColorVisionTypeCount || Index == Visual->ColorVision) return;
	Visual->ColorVision = Index;
	if (auto* Strength = FindSettingsWidget(TEXT("ColorVisionStrengthSetting"))) Strength->SetIsEnabled(Index != 0);
	UpdateVisuals();
}

void UAudioSettingsWidget::SetColorVisionStrength(float Value)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance()))
		Visual->ColorVisionStrength = FMath::Clamp(Value, 0.f, 1.f);
	UpdateVisuals();
}

void UAudioSettingsWidget::ApplyScreenMode()
{
	auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	auto* Mode = Cast<UComboBoxString>(FindSettingsWidget(TEXT("WindowModeSetting")));
	auto* Resolution = Cast<UComboBoxString>(FindSettingsWidget(TEXT("ResolutionSetting")));
	auto* Label = Cast<UTextBlock>(FindSettingsWidget(TEXT("ApplyScreenText")));
	if (!Screen || !Mode || !Resolution || !Label) return;
	if (ScreenConfirmation.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ScreenConfirmation);
		ScreenConfirmation.Reset();
		Screen->ConfirmVideoMode();
		Screen->SaveSettings();
		Mode->SetIsEnabled(true);
		Resolution->SetIsEnabled(Screen->GetFullscreenMode() != EWindowMode::WindowedFullscreen);
		Label->SetText(ApplyScreenLabel);
		RefreshApplyButton();
		return;
	}
	const int32 ModeIndex = Mode->GetSelectedIndex();
	const int32 ResolutionIndex = Resolution->GetSelectedIndex();
	if (ModeIndex < 0 || ModeIndex > 2 || !Resolutions.IsValidIndex(ResolutionIndex)) return;
	PreviousWindowMode = static_cast<int32>(Screen->GetFullscreenMode());
	PreviousResolution = Screen->GetScreenResolution();
	Screen->SetFullscreenMode(static_cast<EWindowMode::Type>(ModeIndex));
	if (ModeIndex != EWindowMode::WindowedFullscreen) Screen->SetScreenResolution(Resolutions[ResolutionIndex]);
	Screen->ApplyResolutionSettings(false);
	Label->SetText(FText::Format(ConfirmCountdownLabel, ScreenConfirmSeconds));
	Mode->SetIsEnabled(false);
	Resolution->SetIsEnabled(false);
	ScreenConfirmDeadline = FPlatformTime::Seconds() + ScreenConfirmSeconds;
	DisplayedSeconds = ScreenConfirmSeconds;
	ScreenConfirmation = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::RevertScreenMode), static_cast<float>(ScreenConfirmSeconds));
}

bool UAudioSettingsWidget::RevertScreenMode(float DeltaTime)
{
	ScreenConfirmation.Reset();
	if (auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		// 엔진의 RevertVideoMode는 마지막으로 "확정"된 값으로 돌아가는데, 한 번도 확정한 적이 없으면 엉뚱한 값일 수 있다.
		// 적용 직전 값을 직접 기억해 두었다가 그대로 되돌린다.
		Screen->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousWindowMode));
		Screen->SetScreenResolution(PreviousResolution);
		Screen->ApplyResolutionSettings(false);
		Screen->ConfirmVideoMode();
		Screen->SaveSettings();
		if (auto* Mode = Cast<UComboBoxString>(FindSettingsWidget(TEXT("WindowModeSetting"))))
		{
			Mode->SetSelectedIndex(static_cast<int32>(Screen->GetFullscreenMode()));
			Mode->SetIsEnabled(true);
		}
		if (auto* Resolution = Cast<UComboBoxString>(FindSettingsWidget(TEXT("ResolutionSetting"))))
		{
			Resolution->SetSelectedIndex(Resolutions.IndexOfByKey(Screen->GetScreenResolution()));
			Resolution->SetIsEnabled(Screen->GetFullscreenMode() != EWindowMode::WindowedFullscreen);
		}
	}
	if (auto* Label = Cast<UTextBlock>(FindSettingsWidget(TEXT("ApplyScreenText"))))
		Label->SetText(ApplyScreenLabel);
	RefreshApplyButton();
	return false;
}
