#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Character/Player/PlayerBase.h"
#include "Map/AreaMapComponent.h"
#include "UI/AreaMapWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Widgets/SWidget.h"

namespace
{
class FAreaMapInputCommand : public IAutomationLatentCommand
{
public:
	explicit FAreaMapInputCommand(FAutomationTestBase* InTest):Test(InTest),Deadline(FPlatformTime::Seconds()+10){}
	virtual bool Update() override
	{
		UWorld* World=GEditor->PlayWorld;
		APlayerController* PC=World ? UGameplayStatics::GetPlayerController(World,0) : nullptr;
		if (!PC && FPlatformTime::Seconds()<Deadline) return false;
		if (!Test->TestNotNull(TEXT("PIE local controller exists"),PC)) return true;
		APlayerBase* MapPlayer=World->SpawnActor<APlayerBase>(FVector(0,0,1000),FRotator::ZeroRotator);
		PC->Possess(MapPlayer);
		UAreaMapComponent* Source=MapPlayer->FindComponentByClass<UAreaMapComponent>();Source->bPersistExploration=false;
		UInputAction* Action=LoadObject<UInputAction>(nullptr,TEXT("/Game/Main/Inputs/Actions/IA_Map.IA_Map"));
		UInputMappingContext* Context=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Main/Inputs/IMC_SideScroller.IMC_SideScroller"));
		if (auto* Inputs=PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Inputs->AddMappingContext(Context,0);
			FModifyContextOptions Options;Options.bForceImmediately=true;
			Inputs->RequestRebuildControlMappings(Options,EInputMappingRebuildType::RebuildWithFlush);
		}
		const bool PreviousCursor=PC->bShowMouseCursor;
		UAreaMapWidget* Screen=CreateWidget<UAreaMapWidget>(PC,UAreaMapWidget::StaticClass());
		Test->TestTrue(TEXT("Area map opens"),Screen->OpenMap(Source,Action));
		Test->TestTrue(TEXT("Area map is displayed and pauses standalone game"),Screen->IsInViewport()&&UGameplayStatics::IsGamePaused(World));
		const FKeyEvent MapKey(EKeys::M,FModifierKeysState(),0,false,0,0);
		Test->TestTrue(TEXT("Mapped M key is consumed by map UI"),Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(),MapKey).IsEventHandled());
		Test->TestFalse(TEXT("Mapped M key closes map and restores pause"),Screen->IsInViewport()||UGameplayStatics::IsGamePaused(World));
		Test->TestEqual(TEXT("Map restores cursor visibility"),PC->bShowMouseCursor,PreviousCursor);
		Screen=CreateWidget<UAreaMapWidget>(PC,UAreaMapWidget::StaticClass());Screen->OpenMap(Source,Action);
		const FKeyEvent Escape(EKeys::Escape,FModifierKeysState(),0,false,0,0);
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(),Escape);
		Test->TestFalse(TEXT("Escape closes map and restores game"),Screen->IsInViewport()||UGameplayStatics::IsGamePaused(World));
		Screen=CreateWidget<UAreaMapWidget>(PC,UAreaMapWidget::StaticClass());Screen->OpenMap(Source,Action);
		Screen->RemoveFromParent();
		Test->TestFalse(TEXT("External removal restores pause"),UGameplayStatics::IsGamePaused(World));
		PC->SetPause(true);
		Screen=CreateWidget<UAreaMapWidget>(PC,UAreaMapWidget::StaticClass());
		Test->TestFalse(TEXT("Map does not take over an existing paused menu"),Screen->OpenMap(Source,Action));
		Test->TestTrue(TEXT("Another menu's pause is preserved"),UGameplayStatics::IsGamePaused(World));
		PC->SetPause(false);
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Deadline;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaMapUITest,"TheLastMessage.Map.OpenCloseAndInputRestore",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAreaMapUITest::RunTest(const FString& Parameters)
{
	UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
	World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FAreaMapInputCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
