#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Framework/SideScrollingPlayerController.h"
#include "UI/PauseMenuWidget.h"
#include "UI/AudioSettingsWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/GameAudioSettingsSubsystem.h"
#include "VolumeSlider.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
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
				auto* Slider = Cast<UVolumeSlider>(SettingsScreen->WidgetTree->FindWidget(Control.Key));
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
#endif
