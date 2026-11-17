#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Framework/SideScrollingPlayerController.h"
#include "UI/PauseMenuWidget.h"
#include "UI/AudioSettingsWidget.h"
#include "Animation/SkeletalMeshActor.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/GameAudioSettingsSubsystem.h"
#include "UI/GameVisualSettingsSubsystem.h"
// +수정----
#include "UI/VolumeSlider.h"
// +--------
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Viewport.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"

namespace
{
class FPauseMenuInputCommand : public IAutomationLatentCommand
{
public:
	explicit FPauseMenuInputCommand(FAutomationTestBase* InTest) : Test(InTest), Deadline(FPlatformTime::Seconds() + 15) {}
	virtual bool Update() override
	{
		UWorld* World = GEditor->PlayWorld;
		ASideScrollingPlayerController* PC = World ? Cast<ASideScrollingPlayerController>(UGameplayStatics::GetPlayerController(World, 0)) : nullptr;
		if ((!PC || !PC->GetPawn()) && FPlatformTime::Seconds() < Deadline) return false;
		if (!Test->TestNotNull(TEXT("Gameplay controller available"), PC)) return true;
		UEnhancedInputLocalPlayerSubsystem* Inputs = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		UEnhancedPlayerInput* PlayerInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
		if (!Test->TestNotNull(TEXT("Enhanced input available"), Inputs)
			|| !Test->TestNotNull(TEXT("Enhanced player input available"), PlayerInput)
			|| !Test->TestNotNull(TEXT("Pause input action configured"), PC->PauseMenuAction.Get())) return true;
		UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Main/Inputs/IMC_SideScroller.IMC_SideScroller"));
		if (!Test->TestNotNull(TEXT("SideScroller input context exists"), Context)) return true;
		Inputs->AddMappingContext(Context, 0);
		FModifyContextOptions Options; Options.bForceImmediately = true;
		Inputs->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
		Test->TestTrue(TEXT("Pause action is enabled during pause"), PC->PauseMenuAction->bTriggerWhenPaused);
		Test->TestTrue(TEXT("SideScroller maps Tab to pause action"),
			Inputs->QueryKeysMappedToAction(PC->PauseMenuAction).Contains(EKeys::Tab));
		const bool PreviousCursor = PC->bShowMouseCursor;
		PlayerInput->InjectInputForAction(PC->PauseMenuAction, FInputActionValue(true));
		PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
		UPauseMenuWidget* Screen = PC->GetPauseMenu();
		if (!Test->TestNotNull(TEXT("Pause UI created"), Screen)) return true;
		Test->TestTrue(TEXT("Menu displays and pauses game"), Screen->IsInViewport() && UGameplayStatics::IsGamePaused(World));
		UWidgetSwitcher* Pages = CastChecked<UWidgetSwitcher>(Screen->WidgetTree->FindWidget(TEXT("PausePages")));
		auto Click = [&](const TCHAR* Name) { CastChecked<UButton>(Screen->WidgetTree->FindWidget(Name))->OnClicked.Broadcast(); };
		const FKeyEvent Tab(EKeys::Tab, FModifierKeysState(), 0, false, 0, 0);
		Test->TestFalse(TEXT("No checkpoint disables reload"), Screen->WidgetTree->FindWidget(TEXT("ReloadButton"))->GetIsEnabled());
		Click(TEXT("SettingsButton"));
		TArray<UUserWidget*> SettingsWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, SettingsWidgets, UAudioSettingsWidget::StaticClass(), true);
		if (!Test->TestEqual(TEXT("Independent settings screen opens"), SettingsWidgets.Num(), 1)) return true;
		UUserWidget* SettingsScreen = SettingsWidgets[0];
		auto* AudioSection = Cast<UUserWidget>(SettingsScreen->WidgetTree->FindWidget(TEXT("AudioSettingsSection")));
		if (!Test->TestNotNull(TEXT("Settings contains the audio Blueprint"), AudioSection)) return true;
		Test->TestNotNull(TEXT("Settings contains the brightness slider"),
			Cast<UAudioSettingsWidget>(SettingsScreen)->FindSettingsWidget(TEXT("BrightnessSetting")));
		auto* Audio = World->GetGameInstance()->GetSubsystem<UGameAudioSettingsSubsystem>();
		const TPair<const TCHAR*, EGameVolume> VolumeControls[] = {
			{TEXT("MasterVolumeSlider"), EGameVolume::Master},
			{TEXT("EffectsVolumeSlider"), EGameVolume::Effects},
			{TEXT("InterfaceVolumeSlider"), EGameVolume::Interface},
			{TEXT("MusicVolumeSlider"), EGameVolume::Music},
			{TEXT("AmbienceVolumeSlider"), EGameVolume::Ambience},
		};
		if (Test->TestNotNull(TEXT("Audio settings are available"), Audio))
		{
			for (const auto& Control : VolumeControls)
			{
				auto* Slider = Cast<UVolumeSlider>(AudioSection->WidgetTree->FindWidget(Control.Key));
				if (!Test->TestNotNull(Control.Key, Slider)) continue;
				const float Previous = Audio->GetVolume(Control.Value);
				Test->TestEqual(TEXT("Slider shows the saved level"), Slider->GetValue(), Previous);
				const float Changed = Previous > 0.5f ? 0.25f : 0.75f;
				Slider->OnValueChanged.Broadcast(Changed);
				Test->TestEqual(TEXT("Slider changes its audio level"), Audio->GetVolume(Control.Value), Changed);
				Test->TestEqual(TEXT("Slider displays the new level"), Slider->GetValue(), Changed);
				Slider->OnToggleMute.ExecuteIfBound();
				Test->TestEqual(TEXT("Mute zeros the slider"), Slider->GetValue(), 0.0f);
				Slider->OnToggleMute.ExecuteIfBound();
				Test->TestEqual(TEXT("Unmute restores the previous level"), Slider->GetValue(), Changed);
				Slider->OnValueChanged.Broadcast(Previous);
			}
		}
		SettingsScreen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Tab);
		Test->TestTrue(TEXT("Tab leaves settings while keeping pause"), !SettingsScreen->IsInViewport() && UGameplayStatics::IsGamePaused(World));
		Click(TEXT("MenuButton")); Test->TestEqual(TEXT("Menu exit requires confirmation"), Pages->GetActiveWidgetIndex(), 1);
		Click(TEXT("CancelButton")); Test->TestEqual(TEXT("Cancel returns to paused menu"), Pages->GetActiveWidgetIndex(), 0);
		Click(TEXT("QuitButton")); Test->TestEqual(TEXT("Desktop exit requires confirmation"), Pages->GetActiveWidgetIndex(), 1);
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Tab);
		Screen->SetCheckpointAvailable(true); Click(TEXT("ReloadButton"));
		Test->TestEqual(TEXT("Checkpoint load requires confirmation"), Pages->GetActiveWidgetIndex(), 1);
		Click(TEXT("CancelButton")); Screen->SetBusy(true);
		Test->TestFalse(TEXT("Save wait disables resume"), Screen->WidgetTree->FindWidget(TEXT("ResumeButton"))->GetIsEnabled());
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Tab);
		Test->TestTrue(TEXT("Tab cannot interrupt pending save navigation"), UGameplayStatics::IsGamePaused(World));
		Screen->SetBusy(false);
		Test->TestTrue(TEXT("Tab is consumed"), Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Tab).IsEventHandled());
		Test->TestFalse(TEXT("Tab resumes game"), UGameplayStatics::IsGamePaused(World));
		Test->TestNull(TEXT("Pause UI removed"), PC->GetPauseMenu());
		Test->TestEqual(TEXT("Cursor state restored"), PC->bShowMouseCursor, PreviousCursor);
		PC->TogglePauseMenu(); Screen = PC->GetPauseMenu();
		if (Test->TestNotNull(TEXT("Menu reopens"), Screen))
		{
			Click(TEXT("ResumeButton"));
			Test->TestFalse(TEXT("Resume button unpauses"), UGameplayStatics::IsGamePaused(World));
		}
		PC->TogglePauseMenu(); Screen = PC->GetPauseMenu();
		if (Screen) Screen->RemoveFromParent();
		Test->TestFalse(TEXT("External removal releases pause"), UGameplayStatics::IsGamePaused(World));
		// Rebind in a transient context, leaving the saved gameplay context untouched.
		Inputs->RemoveMappingContext(Context);
		UInputMappingContext* Remapped = NewObject<UInputMappingContext>(PC);
		Remapped->MapKey(PC->PauseMenuAction, EKeys::F10);
		Inputs->AddMappingContext(Remapped, 100);
		Inputs->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
		PC->TogglePauseMenu(); Screen = PC->GetPauseMenu();
		if (Test->TestNotNull(TEXT("Pause opens with remapped controls"), Screen))
		{
			Test->TestFalse(TEXT("Unmapped Tab is not hardcoded by pause UI"), Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Tab).IsEventHandled());
			const FKeyEvent RemappedKey(EKeys::F10, FModifierKeysState(), 0, false, 0, 0);
			Test->TestTrue(TEXT("Remapped pause key closes the menu"), Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), RemappedKey).IsEventHandled());
			Test->TestNull(TEXT("Remapped pause key resumes gameplay"), PC->GetPauseMenu());
		}
		Inputs->RemoveMappingContext(Remapped);
		Inputs->AddMappingContext(Context, 0);
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Deadline;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMenuUITest, "TheLastMessage.UI.PauseMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPauseMenuUITest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UClass* Mode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/Test_Jihong/Framework/GM_Test.GM_Test_C"));
	if (!TestNotNull(TEXT("Gameplay mode exists"), Mode)) return false;
	World->GetWorldSettings()->DefaultGameMode = Mode;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FPauseMenuInputCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerAppearanceSettingsTabTest, "TheLastMessage.UI.PlayerAppearanceSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerAppearanceSettingsTabTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this]
	{
		UWorld* PlayWorld = GEditor->PlayWorld;
		APlayerController* Controller = PlayWorld ? UGameplayStatics::GetPlayerController(PlayWorld, 0) : nullptr;
		auto* Widget = UAudioSettingsWidget::CreateFor(Controller);
		if (!TestNotNull(TEXT("Settings widget"), Widget)) return;
		const TSharedRef<SWidget> SettingsSlate = Widget->TakeWidget();
		// 하위 탭은 별도 WBP라서 위젯의 부모를 따라 올라가다 페이지 WBP 루트에 닿으면 그 페이지 위젯으로 넘어간다.
		const auto ParentOf = [Widget](UWidget* Child) -> UWidget*
		{
			if (UWidget* Parent = Child->GetParent()) return Parent;
			auto* Page = Child->GetTypedOuter<UUserWidget>();
			return Page && Page != Widget ? Page : nullptr;
		};
		auto* Pages = Cast<UWidgetSwitcher>(Widget->FindSettingsWidget(TEXT("SettingsTabPages")));
		auto* Appearance = Cast<UButton>(Widget->FindSettingsWidget(TEXT("AppearanceSettingsTab")));
		auto* General = Cast<UButton>(Widget->FindSettingsWidget(TEXT("GeneralSettingsTab")));
		auto* AudioTab = Cast<UButton>(Widget->FindSettingsWidget(TEXT("AudioSettingsTab")));
		if (!TestNotNull(TEXT("Settings pages"), Pages) || !TestNotNull(TEXT("Appearance tab"), Appearance)
			|| !TestNotNull(TEXT("General tab"), General) || !TestNotNull(TEXT("Audio tab"), AudioTab)) return;
		TestEqual(TEXT("Graphics tab opens first"), Pages->GetActiveWidgetIndex(), 0);
		for (const TCHAR* Name : {TEXT("GeneralSettingsTabText"), TEXT("AudioSettingsTabText"), TEXT("AppearanceSettingsTabText")})
		{
			auto* Caption = Cast<UTextBlock>(Widget->FindSettingsWidget(Name));
			// 글꼴 줄 높이 때문에 탭 글자는 위로 4픽셀 올려 버튼 가운데에 맞춘다(AudioSettingsWidget의 탭 구성 코드와 같은 값).
			TestTrue(Name, Caption && FMath::IsNearlyEqual(Caption->GetRenderTransform().Translation.Y, -4.f));
		}
		const auto IsOnActivePage = [&](const TCHAR* Name)
		{
			UWidget* Control = Widget->FindSettingsWidget(Name);
			while (Control && Control->GetParent() != Pages) Control = ParentOf(Control);
			return Control && Control == Pages->GetActiveWidget();
		};
		for (const TCHAR* Name : {TEXT("BrightnessSetting"), TEXT("QualityPresetSetting"), TEXT("RenderScaleSetting"), TEXT("BloomSetting"),
			TEXT("ColorVisionSetting"), TEXT("ColorVisionStrengthSetting")})
			TestTrue(FString::Printf(TEXT("%s is on the graphics page"), Name), IsOnActivePage(Name));
		TestFalse(TEXT("Volume is no longer on the graphics page"), IsOnActivePage(TEXT("AudioSettingsSection")));
		if (auto* Screen = GEngine->GetGameUserSettings())
		{
			auto* Quality = Cast<UComboBoxString>(Widget->FindSettingsWidget(TEXT("QualityPresetSetting")));
			auto* RenderScale = Cast<USlider>(Widget->FindSettingsWidget(TEXT("RenderScaleSetting")));
			if (TestNotNull(TEXT("Quality dropdown"), Quality) && TestNotNull(TEXT("Render scale slider"), RenderScale))
			{
				float Normalized, PreviousScale, MinScale, MaxScale;
				Screen->GetResolutionScaleInformationEx(Normalized, PreviousScale, MinScale, MaxScale);
				const int32 PreviousLevel = UAudioSettingsWidget::GetQualityPresetLevel(*Screen);
				RenderScale->OnValueChanged.Broadcast(0.75f);
				float Scale = 0.f;
				Screen->GetResolutionScaleInformationEx(Normalized, Scale, MinScale, MaxScale);
				TestEqual(TEXT("Render scale applies"), Scale, 75.f);
				Quality->OnSelectionChanged.Broadcast(Quality->GetOptionAtIndex(1), ESelectInfo::OnMouseClick);
				TestEqual(TEXT("Quality preset applies"), UAudioSettingsWidget::GetQualityPresetLevel(*Screen), 1);
				Screen->GetResolutionScaleInformationEx(Normalized, Scale, MinScale, MaxScale);
				TestEqual(TEXT("Quality preset keeps the render scale"), Scale, 75.f);
				if (PreviousLevel >= 0) Screen->SetOverallScalabilityLevel(PreviousLevel);
				Screen->SetResolutionScaleValueEx(PreviousScale > 0.f ? PreviousScale : 100.f);
				Screen->ApplyNonResolutionSettings();
			}
		}
		auto* ColorVisionCombo = Cast<UComboBoxString>(Widget->FindSettingsWidget(TEXT("ColorVisionSetting")));
		UWidget* ColorVisionStrength = Widget->FindSettingsWidget(TEXT("ColorVisionStrengthSetting"));
		auto* VisualSettings = PlayWorld->GetGameInstance()->GetSubsystem<UGameVisualSettingsSubsystem>();
		if (TestNotNull(TEXT("Color vision dropdown"), ColorVisionCombo) && TestNotNull(TEXT("Color vision strength"), ColorVisionStrength)
			&& TestNotNull(TEXT("Visual settings for color vision"), VisualSettings))
		{
			const int32 PreviousColorVision = VisualSettings->ColorVision;
			ColorVisionCombo->OnSelectionChanged.Broadcast(ColorVisionCombo->GetOptionAtIndex(2), ESelectInfo::OnMouseClick);
			TestEqual(TEXT("Color vision type applies"), VisualSettings->ColorVision, 2);
			TestTrue(TEXT("Strength is enabled with a filter"), ColorVisionStrength->GetIsEnabled());
			ColorVisionCombo->OnSelectionChanged.Broadcast(ColorVisionCombo->GetOptionAtIndex(0), ESelectInfo::OnMouseClick);
			TestEqual(TEXT("Color vision turns off"), VisualSettings->ColorVision, 0);
			TestFalse(TEXT("Strength is disabled without a filter"), ColorVisionStrength->GetIsEnabled());
			VisualSettings->ColorVision = PreviousColorVision;
			VisualSettings->ApplyDisplay();
		}
		AudioTab->OnClicked.Broadcast();
		TestEqual(TEXT("Audio tab opens"), Pages->GetActiveWidgetIndex(), 1);
		TestTrue(TEXT("Volume sliders are on the audio page"), IsOnActivePage(TEXT("AudioSettingsSection")));
		Appearance->OnClicked.Broadcast();
		TestEqual(TEXT("Appearance tab opens"), Pages->GetActiveWidgetIndex(), 2);
		for (const TCHAR* Name : {TEXT("OutlineColorSetting"), TEXT("OutlineWidthSetting"), TEXT("OutlineBrightnessSetting"), TEXT("PlayerColorSetting")})
		{
			UWidget* Control = Widget->FindSettingsWidget(Name);
			while (Control && Control->GetParent() != Pages) Control = ParentOf(Control);
			TestTrue(Name, Control && Control == Pages->GetActiveWidget());
		}
		auto* Visual = PlayWorld->GetGameInstance()->GetSubsystem<UGameVisualSettingsSubsystem>();
		auto* Brightness = Cast<USlider>(Widget->FindSettingsWidget(TEXT("OutlineBrightnessSetting")));
		auto* Width = Cast<USlider>(Widget->FindSettingsWidget(TEXT("OutlineWidthSetting")));
		auto* Color = Cast<UComboBoxString>(Widget->FindSettingsWidget(TEXT("PlayerColorSetting")));
		auto* Outline = Cast<UComboBoxString>(Widget->FindSettingsWidget(TEXT("OutlineColorSetting")));
		auto* Reset = Cast<UButton>(Widget->FindSettingsWidget(TEXT("ResetAppearanceButton")));
		TestTrue(TEXT("Color dropdowns share the authored style"), Color && Outline
			&& Color->GetWidgetStyle().ComboButtonStyle.ButtonStyle.Normal == Outline->GetWidgetStyle().ComboButtonStyle.ButtonStyle.Normal
			&& Color->GetItemStyle().TextColor == Outline->GetItemStyle().TextColor
			&& Color->GetForegroundColor() == Outline->GetForegroundColor()
			&& Color->GetFont() == Outline->GetFont());
		USkeletalMeshComponent* PreviewMesh = nullptr;
		if (FApp::CanEverRender())
		{
			auto* Box = Cast<USizeBox>(Widget->FindSettingsWidget(TEXT("AppearancePreviewBox")));
			auto* Viewport = Box ? Cast<UViewport>(Box->GetContent()) : nullptr;
			UWorld* PreviewWorld = Viewport ? Viewport->GetViewportWorld() : nullptr;
			auto* Preview = PreviewWorld
				? Cast<ASkeletalMeshActor>(UGameplayStatics::GetActorOfClass(PreviewWorld, ASkeletalMeshActor::StaticClass())) : nullptr;
			if (TestNotNull(TEXT("Appearance tab spawns an isolated preview model"), Preview))
				PreviewMesh = Preview->GetSkeletalMeshComponent();
		}
		if (TestNotNull(TEXT("Visual settings"), Visual) && Brightness && Width && Color && Outline
			&& TestNotNull(TEXT("Appearance defaults button"), Reset))
		{
			const float PreviousBrightness = Visual->OutlineBrightness;
			const float PreviousWidth = Visual->OutlineScale;
			const int32 PreviousColor = Visual->PlayerColor;
			const int32 PreviousOutline = Visual->OutlineColor;
			const FLinearColor PreviousCustomOutline = Visual->CustomOutlineColor;
			const FLinearColor PreviousCustomPlayer = Visual->CustomPlayerColor;
			Width->SetValue(0.70f);
			auto* WidthLabel = Cast<UTextBlock>(Widget->FindSettingsWidget(TEXT("OutlineWidthValue")));
			FNumberFormattingOptions TwoDigits;
			TwoDigits.MinimumFractionalDigits = TwoDigits.MaximumFractionalDigits = 2;
			TestTrue(TEXT("Outline width keeps its trailing zero and a fixed label width"), WidthLabel
				&& WidthLabel->GetText().ToString().StartsWith(FText::AsNumber(0.70f, &TwoDigits).ToString())
				&& Cast<USizeBox>(WidthLabel->GetParent()));
			Brightness->OnValueChanged.Broadcast(1.5f);
			Color->OnSelectionChanged.Broadcast(Color->GetOptionAtIndex(3), ESelectInfo::OnMouseClick);
			TestEqual(TEXT("Outline brightness is independent"), Visual->OutlineBrightness, 1.5f);
			TestEqual(TEXT("Orange player color is selected"), Visual->PlayerColor, 3);
			if (PreviewMesh)
			{
				FLinearColor Tint;
				TestTrue(TEXT("Selected tint appears on the preview mesh"), PreviewMesh->GetMaterial(0)
					&& PreviewMesh->GetMaterial(0)->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color_1")), Tint)
					&& Tint.Equals(Visual->GetPlayerColor()));
				FLinearColor Emission;
				const FLinearColor OutlineColor = Visual->GetOutlineColor(nullptr);
				TestTrue(TEXT("Outline brightness updates the preview"), PreviewMesh->GetOverlayMaterial()
					&& PreviewMesh->GetOverlayMaterial()->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Outline Color")), Emission)
					&& FMath::IsNearlyEqual(Emission.B, OutlineColor.B * Visual->OutlineBrightness));
			}
			Outline->OnSelectionChanged.Broadcast(Outline->GetOptionAtIndex(2), ESelectInfo::OnMouseClick);
			Reset->OnClicked.Broadcast();
			TestEqual(TEXT("Reset restores outline width"), Width->GetValue(), 1.f);
			TestEqual(TEXT("Reset restores outline brightness"), Brightness->GetValue(), 1.f);
			TestEqual(TEXT("Reset restores outline color"), Outline->GetSelectedIndex(), 0);
			TestEqual(TEXT("Reset restores player color"), Color->GetSelectedIndex(), 0);
			Visual->OutlineBrightness = PreviousBrightness;
			Visual->OutlineScale = PreviousWidth;
			Visual->PlayerColor = PreviousColor;
			Visual->OutlineColor = PreviousOutline;
			Visual->CustomOutlineColor = PreviousCustomOutline;
			Visual->CustomPlayerColor = PreviousCustomPlayer;
		}
		General->OnClicked.Broadcast();
		TestEqual(TEXT("Returns to general settings"), Pages->GetActiveWidgetIndex(), 0);
	}, 0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
