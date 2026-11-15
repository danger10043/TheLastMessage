#include "UI/AudioSettingsWidget.h"
#include "UI/FactoryMenuStyle.h"
#include "UI/FactoryUIStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/Font.h"
#include "UI/GameAudioSettingsSubsystem.h"
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
#include "Components/Slider.h"
#include "VolumeSlider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Viewport.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"


#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "Character/Player/PlayerBase.h"
#include "UI/GameVisualSettingsSubsystem.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorPicker.h"


namespace
{
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

TSharedRef<SWidget> UAudioSettingsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout(WidgetTree, true);
	if (WidgetTree) BuildAppearanceTabs();
	return FactoryMenuStyle::WithMenuFocusStyle(Super::RebuildWidget());
}

UAudioSettingsWidget* UAudioSettingsWidget::CreateFor(APlayerController* Player)
{
	if (!Player) return nullptr;
	UClass* Class = LoadClass<UAudioSettingsWidget>(nullptr, TEXT("/Game/Main/UI/Menu/WBP_Settings.WBP_Settings_C"));
	return CreateWidget<UAudioSettingsWidget>(Player, Class ? Class : StaticClass());
}

void UAudioSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* Button = Cast<UButton>(WidgetTree->FindWidget(TEXT("SettingsBackButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Close);
		FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Cancel);
	}
	if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(VolumeRows[0].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetMasterVolume);
	if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(VolumeRows[1].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetMusicVolume);
	if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(VolumeRows[2].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetEffectsVolume);
	if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(VolumeRows[3].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetAmbienceVolume);
	if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(VolumeRows[4].Slider)))
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetInterfaceVolume);
	for (const FVolumeRow& Entry : VolumeRows)
	{
		auto* Slider = Cast<USlider>(WidgetTree->FindWidget(Entry.Slider));
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
	if (auto* Button = Cast<UButton>(WidgetTree->FindWidget(TEXT("GeneralSettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowGeneralSettings);
	if (auto* Button = Cast<UButton>(WidgetTree->FindWidget(TEXT("AppearanceSettingsTab"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAppearanceSettings);
	if (auto* Button = Cast<UButton>(WidgetTree->FindWidget(TEXT("ResetAppearanceButton"))))
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetAppearanceSettings);
		FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Confirm);
	}
}

void UAudioSettingsWidget::FocusFirstControl()
{
	ForceLayoutPrepass();
	auto* Pages = Cast<UWidgetSwitcher>(WidgetTree->FindWidget(TEXT("SettingsTabPages")));
	if (Pages && Pages->GetActiveWidgetIndex() == 1)
	{
		if (auto* Control = WidgetTree->FindWidget(TEXT("OutlineColorSetting"))) Control->SetKeyboardFocus();
	}
	else if (auto* Slider = WidgetTree->FindWidget(TEXT("BrightnessSetting"))) Slider->SetKeyboardFocus();
	else if (auto* Audio = Cast<UAudioSettingsWidget>(WidgetTree->FindWidget(TEXT("AudioSettingsSection"))))
		Audio->FocusFirstControl();
	else if (auto* Volume = WidgetTree->FindWidget(VolumeRows[0].Slider)) Volume->SetKeyboardFocus();
}

void UAudioSettingsWidget::Close()
{
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance())) Audio->SaveVolumes();
	OnClosed.Broadcast();
}

void UAudioSettingsWidget::NativeDestruct()
{
	if (ScreenConfirmation.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ScreenConfirmation);
		RevertScreenMode(0.f);
	}
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance())) Audio->SaveVolumes();
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->Save();
	Super::NativeDestruct();
}

FReply UAudioSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
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
	if (ScreenConfirmation.IsValid())
	{
		const double Remaining = ScreenConfirmDeadline - FPlatformTime::Seconds();
		// 일시정지 중이거나 화면 모드가 바뀌는 동안 코어 티커가 밀릴 수 있어서, 실제 시간 기준으로 여기서 직접 되돌린다.
		if (Remaining <= 0.0)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(ScreenConfirmation);
			RevertScreenMode(0.f);
			FactoryMenuStyle::UpdateFocus(WidgetTree);
			return;
		}
		const int32 Seconds = FMath::Max(1, FMath::CeilToInt(Remaining));
		if (DisplayedSeconds != Seconds)
		{
			DisplayedSeconds = Seconds;
			if (auto* Text = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ApplyScreenText"))))
				Text->SetText(FText::Format(ConfirmCountdownLabel, Seconds));
		}
	}
	FactoryMenuStyle::UpdateFocus(WidgetTree);
}

void UAudioSettingsWidget::RefreshVolumeValues()
{
	const auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance());
	for (const FVolumeRow& Entry : VolumeRows)
	{
		const float Value = Audio ? Audio->GetVolume(Entry.Channel) : 1.0f;
		if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(Entry.Slider))) Slider->SetValue(Value);
		if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(Entry.Percent))) Label->SetText(FText::AsPercent(Value));
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
	if (!Column || Column->GetChildrenCount()) return;
	auto AddRow = [&](const FText& Caption, UWidget* Control) { AddSettingsRow(Tree, Column, Caption, Control); };

	Column->AddChildToVerticalBox(MakeSettingsLabel(Tree, NSLOCTEXT("Settings", "DisplayHeading", "화면"), 24));
	AddRow(NSLOCTEXT("Settings", "Brightness", "밝기"), Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("BrightnessSetting")));
	AddRow(NSLOCTEXT("Settings", "WindowMode", "화면 모드"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("WindowModeSetting")));
	AddRow(NSLOCTEXT("Settings", "Resolution", "해상도"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ResolutionSetting")));
	AddRow(NSLOCTEXT("Settings", "VSync", "수직 동기화"), Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("VSyncSetting")));
	AddRow(NSLOCTEXT("Settings", "FrameLimit", "최대 프레임"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("FrameLimitSetting")));
	AddRow(NSLOCTEXT("Settings", "Shake", "화면 흔들림"), Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ShakeSetting")));
	AddRow(NSLOCTEXT("Settings", "MotionBlur", "모션 블러"), Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("MotionBlurSetting")));
	AddRow(NSLOCTEXT("Settings", "Bloom", "블룸"), Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("BloomSetting")));
	AddRow(NSLOCTEXT("Settings", "OutlineColor", "외곽선 색상"), Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("OutlineColorSetting")));
	AddRow(NSLOCTEXT("Settings", "OutlineWidth", "외곽선 굵기"), Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("OutlineWidthSetting")));
	FactoryMenuStyle::AddButton(Tree, Column, TEXT("ApplyScreenButton"), TEXT("ApplyScreenText"),
		NSLOCTEXT("Settings", "ApplyScreen", "화면 모드 적용"), true);
	auto* Apply = CastChecked<UButton>(Tree->FindWidget(TEXT("ApplyScreenButton")));
	if (auto* Box = Cast<USizeBox>(Apply->GetParent())) Box->SetHeightOverride(44);
}

void UAudioSettingsWidget::BuildAppearanceTabs()
{
	if (WidgetTree->FindWidget(TEXT("SettingsTabPages"))) return;
	auto* Columns = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("SettingsColumns")));
	auto* Display = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("VisualSettingsColumn")));
	UPanelWidget* Parent = Columns ? Columns->GetParent() : nullptr;
	if (!Parent || !Display) return;

	auto* Container = WidgetTree->ConstructWidget<UVerticalBox>();
	if (!Parent->ReplaceChild(Columns, Container)) return;
	Columns->Slot = nullptr;

	auto* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	Container->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	auto AddTab = [&](FName Name, FName TextName, const FText& Caption)
	{
		auto* Cell = WidgetTree->ConstructWidget<UVerticalBox>();
		auto* Slot = Tabs->AddChildToHorizontalBox(Cell);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(4.f, 0.f));
		FactoryMenuStyle::AddButton(WidgetTree, Cell, Name, TextName, Caption, true);
		auto* Button = CastChecked<UButton>(WidgetTree->FindWidget(Name));
		CastChecked<USizeBox>(Button->GetParent())->SetHeightOverride(40.f);
		auto* Text = CastChecked<UTextBlock>(WidgetTree->FindWidget(TextName));
		auto Font = Text->GetFont();
		Font.Size = 22;
		Text->SetFont(Font);
		CastChecked<UButtonSlot>(Text->Slot)->SetVerticalAlignment(VAlign_Center);
		FactoryUI::ApplyButtonSounds(Button, FactoryUI::ESound::Confirm);
	};
	AddTab(TEXT("GeneralSettingsTab"), TEXT("GeneralSettingsTabText"), NSLOCTEXT("Settings", "GeneralTab", "화면 및 소리"));
	AddTab(TEXT("AppearanceSettingsTab"), TEXT("AppearanceSettingsTabText"), NSLOCTEXT("Settings", "AppearanceTab", "플레이어 외형"));
	if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("GeneralSettingsTabText"))))
		Label->SetColorAndOpacity(FactoryUI::Cyan);

	auto* Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("SettingsTabPages"));
	Container->AddChildToVerticalBox(Pages);
	Pages->AddChild(Columns);
	auto* AppearanceColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Pages->AddChild(AppearanceColumns);
	auto* Appearance = WidgetTree->ConstructWidget<UVerticalBox>();
	auto* AppearanceSlot = AppearanceColumns->AddChildToHorizontalBox(Appearance);
	AppearanceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AppearanceSlot->SetVerticalAlignment(VAlign_Top);
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
	MoveRow(TEXT("OutlineColorSetting"), NSLOCTEXT("Settings", "OutlineColor", "외곽선 색상"), UComboBoxString::StaticClass());
	MoveRow(TEXT("OutlineWidthSetting"), NSLOCTEXT("Settings", "OutlineWidth", "외곽선 굵기"), USlider::StaticClass());
	AddSettingsRow(WidgetTree, Appearance, NSLOCTEXT("Settings", "OutlineBrightness", "외곽선 밝기"),
		WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("OutlineBrightnessSetting")));
	const FName PlayerColorName(TEXT("PlayerColorSetting"));
	auto* OutlineColor = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("OutlineColorSetting")));
	auto* PlayerColor = OutlineColor ? DuplicateObject<UComboBoxString>(OutlineColor, WidgetTree, PlayerColorName)
		: WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), PlayerColorName);
	PlayerColor->Slot = nullptr;
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

	if (FApp::CanEverRender())
	{
		auto* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrush(FactoryUI::Chamfer());
		Frame->SetPadding(FMargin(8.f));
		auto* PreviewBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AppearancePreviewBox"));
		PreviewBox->SetWidthOverride(340.f);
		PreviewBox->SetHeightOverride(190.f);
		Frame->SetContent(PreviewBox);
		auto* PreviewSlot = AppearanceColumns->AddChildToHorizontalBox(Frame);
		PreviewSlot->SetVerticalAlignment(VAlign_Top);
		PreviewSlot->SetPadding(FMargin(36.f, 6.f, 0.f, 0.f));
	}
}

void UAudioSettingsWidget::ShowSettingsTab(int32 Index)
{
	if (auto* Pages = Cast<UWidgetSwitcher>(WidgetTree->FindWidget(TEXT("SettingsTabPages"))))
	{
		Pages->SetActiveWidgetIndex(Index);
		if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("GeneralSettingsTabText"))))
			Label->SetColorAndOpacity(Index == 0 ? FactoryUI::Cyan : FactoryUI::Text);
		if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AppearanceSettingsTabText"))))
			Label->SetColorAndOpacity(Index == 1 ? FactoryUI::Cyan : FactoryUI::Text);
		if (Index == 1) RefreshAppearancePreview();
		FocusFirstControl();
	}
}

void UAudioSettingsWidget::ShowGeneralSettings() { ShowSettingsTab(0); }
void UAudioSettingsWidget::ShowAppearanceSettings() { ShowSettingsTab(1); }

void UAudioSettingsWidget::ResetAppearanceSettings()
{
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Visual) return;
	Visual->ResetAppearance();
	{
		TGuardValue<bool> Guard(bSynchronizingAppearanceControls, true);
		for (const TCHAR* Name : {TEXT("OutlineColorSetting"), TEXT("PlayerColorSetting")})
			if (auto* Combo = Cast<UComboBoxString>(WidgetTree->FindWidget(Name))) Combo->SetSelectedIndex(0);
		if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(TEXT("OutlineWidthSetting")))) Slider->SetValue(Visual->OutlineScale);
		if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(TEXT("OutlineBrightnessSetting")))) Slider->SetValue(Visual->OutlineBrightness);
	}
	UpdateVisuals();
}

void UAudioSettingsWidget::RefreshAppearancePreview()
{
	auto* Box = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("AppearancePreviewBox")));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Box || !Visual) return;

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
		if (!Viewport)
		{
			Viewport = WidgetTree->ConstructWidget<UViewport>();
			Viewport->SetBackgroundColor(FLinearColor::FromSRGBColor(FColor(13, 30, 42)));
			Viewport->SetVisibility(ESlateVisibility::HitTestInvisible);
			Box->SetContent(Viewport);
		}
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
		const double Distance = FMath::Max(3.0 * Bounds.BoxExtent.Z, 2.1 * Bounds.SphereRadius);
		Viewport->SetViewLocation(Bounds.Origin + Distance * Direction);
		Viewport->SetViewRotation((-Direction).Rotation());
		Viewport->SetLightIntensity(2.0f);
		Viewport->SetSkyIntensity(1.2f);
	}
	Visual->ApplyPreviewAppearance(Mesh, AppearancePreviewBody);
}

void UAudioSettingsWidget::BuildVisualControls()
{
	auto* Column = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("VisualSettingsColumn")));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Column || !Visual || !Screen) return;
	Resolutions.Reset();

	auto AddSlider = [&](FName Name, float Value, float Maximum)
	{
		auto* Slider = Cast<USlider>(WidgetTree->FindWidget(Name));
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
		auto* Combo = Cast<UComboBoxString>(WidgetTree->FindWidget(Name));
		if (!Combo) return Combo;
		Combo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::MakeDropdownOption);
		Combo->ClearOptions();
		for (const FString& Option : Options) Combo->AddOption(Option);
		Combo->SetSelectedIndex(Selected);
		return Combo;
	};
	auto AddToggle = [&](FName Name, bool bValue)
	{
		auto* Check = Cast<UCheckBox>(WidgetTree->FindWidget(Name));
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
		if (BrightnessSlider->GetToolTipText().IsEmpty()) BrightnessSlider->SetToolTipText(BrightnessHelpText);
		BrightnessSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetBrightness);
	}
	if (auto* Mode = AddCombo(TEXT("WindowModeSetting"),
		{TEXT("전체 화면"), TEXT("테두리 없는 창"), TEXT("창 모드")}, static_cast<int32>(Screen->GetFullscreenMode())))
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
	const float Limit = Screen->GetFrameRateLimit();
	if (auto* FrameLimit = AddCombo(TEXT("FrameLimitSetting"),
		{TEXT("제한 없음"), TEXT("30"), TEXT("60"), TEXT("120")}, Limit == 30 ? 1 : Limit == 60 ? 2 : Limit == 120 ? 3 : 0))
		FrameLimit->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetFrameLimit);
	if (auto* Shake = AddSlider(TEXT("ShakeSetting"), Visual->ShakeScale, 1.f))
		Shake->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetShake);
	if (auto* MotionBlur = AddToggle(TEXT("MotionBlurSetting"), Visual->bMotionBlur))
		MotionBlur->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::SetMotionBlur);
	if (auto* Bloom = AddToggle(TEXT("BloomSetting"), Visual->bBloom))
		Bloom->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::SetBloom);
	const TArray<FString> AppearanceColors = {TEXT("기본"), TEXT("흰색"), TEXT("청록"), TEXT("주황"),
		TEXT("분홍"), TEXT("연두"), TEXT("사용자 지정"), TEXT("팔레트 열기…")};
	if (auto* OutlineColor = AddCombo(TEXT("OutlineColorSetting"), AppearanceColors, Visual->OutlineColor))
		OutlineColor->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetOutlineColor);
	if (auto* PlayerColor = AddCombo(TEXT("PlayerColorSetting"), AppearanceColors, Visual->PlayerColor))
		PlayerColor->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::SetPlayerColor);
	if (auto* OutlineWidth = AddSlider(TEXT("OutlineWidthSetting"), Visual->OutlineScale, 2.f))
		OutlineWidth->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetOutlineWidth);
	if (auto* OutlineBrightness = AddSlider(TEXT("OutlineBrightnessSetting"), Visual->OutlineBrightness, 2.f))
		OutlineBrightness->OnValueChanged.AddUniqueDynamic(this, &ThisClass::SetOutlineBrightness);
	if (auto* Apply = Cast<UButton>(WidgetTree->FindWidget(TEXT("ApplyScreenButton"))))
		Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplyScreenMode);
	RefreshVisualReadouts();
	RefreshApplyButton();
}

UWidget* UAudioSettingsWidget::MakeDropdownOption(FString Option)
{
	// 글꼴과 색은 그 콤보박스의 디자이너 값(Font, Foreground Color, Item Style의 Text Color)을 그대로 쓴다.
	// 코드는 F_TLM_UI의 글자가 줄 높이보다 아래로 그려지는 것만 보정한다(라벨의 -5 오프셋과 같은 이유).
	const UComboBoxString* Source = nullptr;
	for (const TCHAR* Name : {TEXT("WindowModeSetting"), TEXT("ResolutionSetting"), TEXT("FrameLimitSetting"),
		TEXT("OutlineColorSetting"), TEXT("PlayerColorSetting")})
		if (const auto* Combo = Cast<UComboBoxString>(WidgetTree->FindWidget(Name)); Combo && Combo->FindOptionIndex(Option) >= 0)
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
	const FReadout Entries[] = {
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
		auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(Entry.Name));
		if (!Label)
		{
			auto* Control = WidgetTree->FindWidget(Entry.Control);
			auto* Row = Control ? Cast<UHorizontalBox>(Control->GetParent()) : nullptr;
			if (!Row) continue;
			Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Entry.Name);
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
				auto* FixedWidth = WidgetTree->ConstructWidget<USizeBox>();
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
			Label->SetToolTipText(FText::FromString(TEXT("#") + Color.ToFColorSRGB().ToHex().Left(6)));
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
	auto* Combo = Cast<UComboBoxString>(WidgetTree->FindWidget(ComboName));
	auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
	if (!Combo || !Visual) return;
	int32& SelectedColor = bOutline ? Visual->OutlineColor : Visual->PlayerColor;
	const int32 Index = Combo->FindOptionIndex(Value);
	if (Index > UGameVisualSettingsSubsystem::CustomColorIndex)
	{
		Combo->SetSelectedIndex(SelectedColor);
		const FLinearColor InitialColor = bOutline
			? Visual->GetOutlineColor(Cast<APlayerBase>(GetOwningPlayerPawn()))
			: Visual->GetPlayerColor();
		FColorPickerArgs Args(InitialColor,
			FOnLinearColorValueChanged::CreateWeakLambda(this, [this, ComboName, bOutline](FLinearColor Color)
			{
				auto* Settings = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance());
				if (!Settings) return;
				(bOutline ? Settings->CustomOutlineColor : Settings->CustomPlayerColor) = Color.GetClamped();
				(bOutline ? Settings->OutlineColor : Settings->PlayerColor) = UGameVisualSettingsSubsystem::CustomColorIndex;
				if (auto* Picker = Cast<UComboBoxString>(WidgetTree->FindWidget(ComboName)))
					Picker->SetSelectedIndex(UGameVisualSettingsSubsystem::CustomColorIndex);
				UpdateVisuals();
			}));
		Args.ParentWidget = TakeWidget();
		Args.bIsModal = true;
		Args.bOnlyRefreshOnOk = true;
		Args.bClampValue = true;
		Args.OnColorPickerCancelled.BindLambda([](FLinearColor) {});
		OpenColorPicker(Args);
		return;
	}
	if (Index < 0) return;
	SelectedColor = Index;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetMotionBlur(bool bValue)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->bMotionBlur = bValue;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetBloom(bool bValue)
{
	if (auto* Visual = UGameInstance::GetSubsystem<UGameVisualSettingsSubsystem>(GetGameInstance())) Visual->bBloom = bValue;
	UpdateVisuals();
}

void UAudioSettingsWidget::SetWindowMode(FString Value, ESelectInfo::Type Selection)
{
	auto* Mode = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("WindowModeSetting")));
	auto* Resolution = WidgetTree->FindWidget(TEXT("ResolutionSetting"));
	if (Mode && Resolution) Resolution->SetIsEnabled(Mode->FindOptionIndex(Value) != static_cast<int32>(EWindowMode::WindowedFullscreen));
	RefreshApplyButton();
}

void UAudioSettingsWidget::OnResolutionChanged(FString Value, ESelectInfo::Type Selection) { RefreshApplyButton(); }

void UAudioSettingsWidget::RefreshApplyButton()
{
	auto* Apply = Cast<UButton>(WidgetTree->FindWidget(TEXT("ApplyScreenButton")));
	auto* Mode = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("WindowModeSetting")));
	auto* Resolution = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("ResolutionSetting")));
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
	if (auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Screen->SetFrameRateLimit(FCString::Atof(*Value));
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

void UAudioSettingsWidget::ApplyScreenMode()
{
	auto* Screen = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	auto* Mode = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("WindowModeSetting")));
	auto* Resolution = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("ResolutionSetting")));
	auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ApplyScreenText")));
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
		if (auto* Mode = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("WindowModeSetting"))))
		{
			Mode->SetSelectedIndex(static_cast<int32>(Screen->GetFullscreenMode()));
			Mode->SetIsEnabled(true);
		}
		if (auto* Resolution = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("ResolutionSetting"))))
		{
			Resolution->SetSelectedIndex(Resolutions.IndexOfByKey(Screen->GetScreenResolution()));
			Resolution->SetIsEnabled(Screen->GetFullscreenMode() != EWindowMode::WindowedFullscreen);
		}
	}
	if (auto* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ApplyScreenText"))))
		Label->SetText(ApplyScreenLabel);
	RefreshApplyButton();
	return false;
}
