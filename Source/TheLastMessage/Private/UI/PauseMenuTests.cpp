#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Framework/SideScrollingPlayerController.h"
#include "UI/PauseMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"

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
		const bool PreviousCursor = PC->bShowMouseCursor;
		PC->TogglePauseMenu();
		UPauseMenuWidget* Screen = PC->GetPauseMenu();
		if (!Test->TestNotNull(TEXT("Pause UI created"), Screen)) return true;
		Test->TestTrue(TEXT("Menu displays and pauses game"), Screen->IsInViewport() && UGameplayStatics::IsGamePaused(World));
		UWidgetSwitcher* Pages = CastChecked<UWidgetSwitcher>(Screen->WidgetTree->FindWidget(TEXT("PausePages")));
		auto Click = [&](const TCHAR* Name) { CastChecked<UButton>(Screen->WidgetTree->FindWidget(Name))->OnClicked.Broadcast(); };
		const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
		Test->TestFalse(TEXT("No checkpoint disables reload"), Screen->WidgetTree->FindWidget(TEXT("ReloadButton"))->GetIsEnabled());
		Click(TEXT("SettingsButton")); Test->TestEqual(TEXT("Settings page opens"), Pages->GetActiveWidgetIndex(), 2);
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Escape);
		Test->TestTrue(TEXT("Escape leaves settings while keeping pause"), Pages->GetActiveWidgetIndex() == 0 && UGameplayStatics::IsGamePaused(World));
		Click(TEXT("MenuButton")); Test->TestEqual(TEXT("Menu exit requires confirmation"), Pages->GetActiveWidgetIndex(), 1);
		Click(TEXT("CancelButton")); Test->TestEqual(TEXT("Cancel returns to paused menu"), Pages->GetActiveWidgetIndex(), 0);
		Click(TEXT("QuitButton")); Test->TestEqual(TEXT("Desktop exit requires confirmation"), Pages->GetActiveWidgetIndex(), 1);
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Escape);
		Screen->SetCheckpointAvailable(true); Click(TEXT("ReloadButton"));
		Test->TestEqual(TEXT("Checkpoint load requires confirmation"), Pages->GetActiveWidgetIndex(), 1);
		Click(TEXT("CancelButton")); Screen->SetBusy(true);
		Test->TestFalse(TEXT("Save wait disables resume"), Screen->WidgetTree->FindWidget(TEXT("ResumeButton"))->GetIsEnabled());
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Escape);
		Test->TestTrue(TEXT("Escape cannot interrupt pending save navigation"), UGameplayStatics::IsGamePaused(World));
		Screen->SetBusy(false);
		Test->TestTrue(TEXT("Escape is consumed"), Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Escape).IsEventHandled());
		Test->TestFalse(TEXT("Escape resumes game"), UGameplayStatics::IsGamePaused(World));
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
