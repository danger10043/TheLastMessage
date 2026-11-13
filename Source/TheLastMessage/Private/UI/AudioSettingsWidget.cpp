#include "UI/AudioSettingsWidget.h"
#include "UI/FactoryMenuStyle.h"
#include "UI/FactoryUIStyle.h"
#include "UI/FactoryUIAssetSettings.h"
#include "Engine/Font.h"
#include "UI/GameAudioSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "VolumeSlider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"


#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"


namespace
{
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
		FSliderStyle Style = Slider->GetWidgetStyle();
		Style.NormalBarImage = FSlateRoundedBoxBrush(FLinearColor(0.17f, 0.23f, 0.26f), 1.0f);
		Style.HoveredBarImage = Style.NormalBarImage;
		Style.BarThickness = 4.0f;
		Style.NormalThumbImage = FSlateRoundedBoxBrush(FLinearColor::White, 1.0f, FVector2f(11, 18));
		Style.HoveredThumbImage = Style.NormalThumbImage;
		Slider->SetWidgetStyle(Style);
		Slider->SetIndentHandle(false);
		Slider->SetToolTipText(NSLOCTEXT("Pause", "VolumeMuteHint", "손잡이 클릭으로 음소거 또는 이전 볼륨 복원"));
		Slider->SetSliderHandleColor(FactoryUI::Text);
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

void UAudioSettingsWidget::BuildDefaultLayout(UWidgetTree* Tree)
{
	auto* Pages = FactoryMenuStyle::BuildShell(Tree, true);
	auto* Panel = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AudioSettingsPanel"));
	Pages->AddChild(Panel);
	auto* Heading = FactoryMenuStyle::AddText(Tree, Panel, TEXT("SettingsHeading"), NSLOCTEXT("Pause", "Settings", "설정"), 32);
	auto Font = Heading->GetFont();
	if (UFont* Title = GetDefault<UFactoryUIAssetSettings>()->TitleFont.LoadSynchronous()) Font.FontObject = Title;
	Heading->SetFont(Font);
	AddVolumeRows(Tree, Panel);
	FactoryMenuStyle::AddButton(Tree, Panel, TEXT("SettingsBackButton"), TEXT("SettingsBackText"), NSLOCTEXT("Pause", "Back", "뒤로"), true);
	if (auto* Hint = Cast<UTextBlock>(Tree->FindWidget(TEXT("NavigationHint"))))
		Hint->SetText(NSLOCTEXT("Pause", "VolumeNavigation", "↑↓ 선택    ←→ 볼륨 조절"));
}

TSharedRef<SWidget> UAudioSettingsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout(WidgetTree);
	return FactoryMenuStyle::WithMenuFocusStyle(Super::RebuildWidget());
}

UAudioSettingsWidget* UAudioSettingsWidget::CreateFor(APlayerController* Player)
{
	if (!Player) return nullptr;
	UClass* Class = LoadClass<UAudioSettingsWidget>(nullptr, TEXT("/Game/Main/UI/Menu/WBP_AudioSettings.WBP_AudioSettings_C"));
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
		if (auto* Slider = Cast<UVolumeSlider>(WidgetTree->FindWidget(Entry.Slider)))
			Slider->OnToggleMute.BindWeakLambda(this, [this, Channel = Entry.Channel]
			{
				if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance()))
				{
					Audio->ToggleMute(Channel);
					RefreshVolumeValues();
				}
			});

	RefreshVolumeValues();
}

void UAudioSettingsWidget::FocusFirstControl()
{
	ForceLayoutPrepass();
	if (auto* Slider = WidgetTree->FindWidget(VolumeRows[0].Slider)) Slider->SetKeyboardFocus();
}

void UAudioSettingsWidget::Close()
{
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance())) Audio->SaveVolumes();
	OnClosed.Broadcast();
}

void UAudioSettingsWidget::NativeDestruct()
{
	if (auto* Audio = UGameInstance::GetSubsystem<UGameAudioSettingsSubsystem>(GetGameInstance())) Audio->SaveVolumes();
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
	FactoryMenuStyle::UpdateFocus(WidgetTree);
		for (const FVolumeRow& Entry : VolumeRows)
			if (auto* Slider = Cast<USlider>(WidgetTree->FindWidget(Entry.Slider)))
			{
				const bool bSelected = Slider->HasAnyUserFocus() || Slider->IsHovered();
				const FLinearColor Color = bSelected ? FactoryUI::Amber : FactoryUI::Text;
				if (Slider->GetSliderHandleColor() != Color)
				{
					Slider->SetSliderHandleColor(Color);
					if (auto* Percent = Cast<UTextBlock>(WidgetTree->FindWidget(Entry.Percent)))
						Percent->SetColorAndOpacity(Color);
				}
			}
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


