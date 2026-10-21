#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/InputComponent.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WidgetSwitcher.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "TextureCompiler.h"
#include "UI/OwnedCoreWidget.h"
#include "UI/OwnedCoreViewerSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"
#include "Widgets/SViewport.h"

namespace
{
class FOwnedCoreLifecycleCommand : public IAutomationLatentCommand
{
public:
	explicit FOwnedCoreLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest), Deadline(FPlatformTime::Seconds() + 20.) {}
	virtual bool Update() override
	{
		UWorld* World = GEditor->PlayWorld;
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		if (!PC)
		{
			if (FPlatformTime::Seconds() < Deadline) return false;
			Test->AddError(TEXT("PIE controller is unavailable")); return true;
		}
		ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
		UOwnedCoreViewerSubsystem* Menu = LocalPlayer ? LocalPlayer->GetSubsystem<UOwnedCoreViewerSubsystem>() : nullptr;
		if (!Test->TestNotNull(TEXT("Owned menu subsystem starts automatically"), Menu)) return true;
		APlayerBase* Player = World->SpawnActor<APlayerBase>(FVector(0, 0, 1000), FRotator::ZeroRotator);
		if (!Test->TestNotNull(TEXT("Player exists"), Player)) return true;
		APawn* Previous = PC->GetPawn();
		PC->Possess(Player);
		if (Previous) Previous->Destroy();
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		UCoreComponent* Core = Player->GetCoreComponent();
		if (!Test->TestNotNull(TEXT("ASC exists"), ASC) || !Test->TestNotNull(TEXT("Core component exists"), Core)) return true;
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetMaxHealthAttribute(), 100);
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 100);
		Core->ResetOwnedCores();
		UGameViewportClient* Viewport = World->GetGameViewport();
		if (!Test->TestNotNull(TEXT("Game viewport exists"), Viewport)) return true;
		const TSharedPtr<SWidget> PreviousFocus = Viewport->GetGameViewportWidget();
		FInputModeGameAndUI PriorMode;
		PriorMode.SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways).SetWidgetToFocus(PreviousFocus);
		PC->SetInputMode(PriorMode);
		// 캡처 설정을 별도로 바꿔도 GameAndUI의 포커스 정책은 보존되어야 한다.
		Viewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
		const bool PreviousCursor = PC->bShowMouseCursor;
		bool bInvokedInput = false;
		TInlineComponentArray<UInputComponent*> Inputs(PC);
		for (UInputComponent* Input : Inputs)
		{
			if (Input->Priority != 10) continue;
			for (FInputKeyBinding& Binding : Input->KeyBindings)
			{
				if (Binding.Chord.Key == EKeys::I && Binding.KeyEvent == IE_Pressed)
				{
					Binding.KeyDelegate.Execute(EKeys::I);
					bInvokedInput = true;
				}
			}
		}
		Test->TestTrue(TEXT("The controller has a bound I key"), bInvokedInput);
		Test->TestTrue(TEXT("The I key opens an empty owned menu"), Menu->IsViewerOpen());
		UOwnedCoreWidget* Screen = FindScreen(PC);
		if (!Test->TestNotNull(TEXT("Menu WBP is in the viewport"), Screen)) return true;
		Test->TestTrue(TEXT("Opening pauses gameplay"), World->IsPaused());
		Test->TestTrue(TEXT("Opening exposes the mouse cursor"), PC->bShowMouseCursor);
		Test->TestFalse(TEXT("Opening twice is rejected"), Menu->OpenViewer());
		UWidgetSwitcher* Pages = CastChecked<UWidgetSwitcher>(Screen->GetWidgetFromName(TEXT("CategoryPages")));
		Test->TestEqual(TEXT("Empty viewer opens the ability tab"), Pages->GetActiveWidgetIndex(), 0);
		Test->TestTrue(TEXT("Both tabs remain available when empty"), Screen->GetWidgetFromName(TEXT("AbilityTab"))->IsVisible() && Screen->GetWidgetFromName(TEXT("StatTab"))->IsVisible());
		Test->TestTrue(TEXT("The outer detail rectangle remains visible when empty"), CastChecked<UBorder>(Screen->GetWidgetFromName(TEXT("DetailPanel")))->GetBrushColor().A > 0.f);
		Test->TestNull(TEXT("There is no separate rectangle around only the drawing"), Screen->GetWidgetFromName(TEXT("DrawingFrame")));
		Test->TestFalse(TEXT("Empty details contain no text block"), Screen->GetWidgetFromName(TEXT("CoreDetails"))->IsVisible());
		Test->TestFalse(TEXT("Empty details have no placeholder core icon"), Screen->GetWidgetFromName(TEXT("DetailIcon"))->IsVisible());
		for (const TCHAR* Name : {TEXT("Eyebrow"), TEXT("ModuleCount"), TEXT("EmptyText"), TEXT("StatsTitle"),
			TEXT("CategoryRow"), TEXT("EffectKey"), TEXT("NextKey"), TEXT("CoreCategory"),
			TEXT("DrawingLineSize"), TEXT("CategoryTopLineSize"), TEXT("EffectTopLineSize"), TEXT("NextTopLineSize")})
			Test->TestNull(FString::Printf(TEXT("Unused UI %s is removed"), Name), Screen->GetWidgetFromName(Name));
		Test->TestTrue(TEXT("Stat labels and values remain visible without a heading"), Screen->GetWidgetFromName(TEXT("StatsRow"))->IsVisible());
		Test->TestEqual(TEXT("The empty viewer still shows the actual max health"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("MaxHealthValue")))->GetText().ToString(), FString(TEXT("100")));
		Test->TestNull(TEXT("Decorative drawing code is removed"), Screen->GetWidgetFromName(TEXT("DrawingCode")));
		Test->TestNull(TEXT("Footer key hint is removed"), Screen->GetWidgetFromName(TEXT("CloseHint")));
		Test->TestEqual(TEXT("The close button contains only its symbol"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("CloseLabel")))->GetText().ToString(), FString(TEXT("×")));
		UGameplayStatics::SetGamePaused(World, false);
		Test->TestTrue(TEXT("Other requests cannot unpause an open menu"), World->IsPaused());

		UCoreDataAsset* Frame = LoadObject<UCoreDataAsset>(nullptr, TEXT("/Game/Test_Jihun/Core/Stat/DataAsset/DA_Core_ReinforcedFrame.DA_Core_ReinforcedFrame"));
		UCoreDataAsset* Hook = LoadObject<UCoreDataAsset>(nullptr, TEXT("/Game/Test_Jihun/Core/Ability/DataAsset/DA_Core_GrapplingHook.DA_Core_GrapplingHook"));
		if (!Test->TestNotNull(TEXT("Real stat DA exists"), Frame) || !Test->TestNotNull(TEXT("Real ability DA exists"), Hook)) return true;
		CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("StatTab")))->OnClicked.Broadcast();
		Test->TestEqual(TEXT("An empty stat tab can be selected"), Pages->GetActiveWidgetIndex(), 1);
		Test->TestTrue(TEXT("Actual frame can be acquired"), IsCoreAcquireSuccess(Core->AcquireCore(Frame)));
		Test->TestTrue(TEXT("Actual hook can be acquired"), IsCoreAcquireSuccess(Core->AcquireCore(Hook)));
		Test->TestEqual(TEXT("Stat category has exactly one acquired core"), CastChecked<UWrapBox>(Screen->GetWidgetFromName(TEXT("StatCoresBox")))->GetChildrenCount(), 1);
		Test->TestEqual(TEXT("Ability category has exactly one acquired core"), CastChecked<UWrapBox>(Screen->GetWidgetFromName(TEXT("AbilityCoresBox")))->GetChildrenCount(), 1);
		Test->TestEqual(TEXT("An acquisition preserves the selected tab"), Pages->GetActiveWidgetIndex(), 1);
		CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("AbilityTab")))->OnClicked.Broadcast();
		Test->TestEqual(TEXT("Clicking the ability tab switches the list"), Pages->GetActiveWidgetIndex(), 0);
		Test->TestEqual(TEXT("Ability tab displays the owned ability detail"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("CoreName")))->GetText().ToString(), Hook->CoreName.ToString());
		Test->TestTrue(TEXT("The active tab is marked"), Screen->GetWidgetFromName(TEXT("AbilityTabIndicator"))->IsVisible());
		Test->TestFalse(TEXT("The inactive tab has no active marker"), Screen->GetWidgetFromName(TEXT("StatTabIndicator"))->IsVisible());
		CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("StatTab")))->OnClicked.Broadcast();
		Test->TestTrue(TEXT("Upgrade succeeds while the UI listens"), Core->AcquireCore(Frame) == ECoreAcquireResult::Upgraded);
		Test->TestEqual(TEXT("An upgrade does not duplicate its icon"), CastChecked<UWrapBox>(Screen->GetWidgetFromName(TEXT("StatCoresBox")))->GetChildrenCount(), 1);
		Test->TestTrue(TEXT("Details show current level two"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("CoreLevel")))->GetText().ToString().Contains(TEXT("Lv.2")));
		const FString Current = CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("CurrentEffect")))->GetText().ToString();
		Test->TestTrue(TEXT("Current effect has no next-level comparison"), Current.Contains(TEXT("30")) && !Current.Contains(TEXT("→")));
		Test->TestEqual(TEXT("A real core upgrade updates the max health summary"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("MaxHealthValue")))->GetText().ToString(), FString(TEXT("130")));
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetCriticalChanceAttribute(), .2f);
		ASC->SetNumericAttributeBase(UWeaponSlotAttributeSet::GetSharedMaxEnergyAttribute(), 150.f);
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetMoveSpeedMultiplierAttribute(), 1.42f);
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetDamageDealtMultiplierAttribute(), 1.25f);
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetDamageTakenMultiplierAttribute(), .9f);
		for (const auto& Expected : TArray<TPair<FName, FString>>{
			{TEXT("CriticalChanceValue"), TEXT("20%")}, {TEXT("MaxEnergyValue"), TEXT("150")},
			{TEXT("MoveSpeedValue"), TEXT("+42%")}, {TEXT("DamageDealtValue"), TEXT("+25%")}, {TEXT("DamageTakenValue"), TEXT("-10%")}})
			Test->TestEqual(FString::Printf(TEXT("%s updates from the actual ASC"), *Expected.Key.ToString()),
				CastChecked<UTextBlock>(Screen->GetWidgetFromName(Expected.Key))->GetText().ToString(), Expected.Value);

		FWidgetRenderer Renderer(true);
		auto Render = [&](const FString& Filename, FVector2D Size)
		{
			TSharedRef<SWidget> Slate = Screen->TakeWidget();
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(Slate, Size);
			if (!Test->TestNotNull(TEXT("Menu renders offscreen"), Target)) return;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				++GFrameCounter;
				Renderer.DrawWidget(Target, Slate, Size, .016f);
				FlushRenderingCommands();
			}
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/OwnedCore") / Filename;
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			FBufferArchive PNG;
			Test->TestTrue(TEXT("Menu screenshot is saved"), FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG) && FFileHelper::SaveArrayToFile(PNG, *Path));
		};
		TArray<UTexture*> Textures;
		for (const TCHAR* Name : {TEXT("DetailIcon"), TEXT("DetailGrid"), TEXT("ScreenGrid")})
		{
			if (UImage* Image = Cast<UImage>(Screen->GetWidgetFromName(Name)))
				if (UTexture* Texture = Cast<UTexture>(Image->GetBrush().GetResourceObject())) Textures.AddUnique(Texture);
		}
		FTextureCompilingManager::Get().FinishCompilation(Textures);
		Render(TEXT("Owned.png"), FVector2D(1920, 1080));
		const auto CheckLayout = [&](FVector2D Size)
		{
			for (const TCHAR* Name : {TEXT("HeaderLineSize"), TEXT("StatsLineSize"), TEXT("FooterLineSize"), TEXT("StatTabIndicatorSize")})
			{
				const FGeometry& Geometry = Screen->GetWidgetFromName(Name)->GetCachedGeometry();
				const float PixelHeight = Geometry.LocalToAbsolute(FVector2D(0, Geometry.GetLocalSize().Y)).Y - Geometry.GetAbsolutePosition().Y;
				Test->TestTrue(FString::Printf(TEXT("%s is at least one pixel at %.0fx%.0f"), Name, Size.X, Size.Y), PixelHeight >= 1.f);
			}
			for (const TCHAR* Name : {TEXT("CoreLevel"), TEXT("CloseLabel"), TEXT("AbilityTabLabel"), TEXT("StatTabLabel")})
			{
				const UTextBlock* Label = CastChecked<UTextBlock>(Screen->GetWidgetFromName(Name));
				Test->TestTrue(FString::Printf(TEXT("%s stays on one line"), Name), Label->GetDesiredSize().Y <= Label->GetFont().Size * 1.8f);
			}
			const FGeometry& Footer = Screen->GetWidgetFromName(TEXT("Footer"))->GetCachedGeometry();
			Test->TestTrue(TEXT("Footer stays inside the rendered screen"), Footer.LocalToAbsolute(Footer.GetLocalSize()).Y <= Size.Y + .1f);
			const FGeometry& Details = Screen->GetWidgetFromName(TEXT("DetailPanel"))->GetCachedGeometry();
			const FGeometry& Stats = Screen->GetWidgetFromName(TEXT("StatsRow"))->GetCachedGeometry();
			Test->TestTrue(TEXT("Details fit above the stat summary"), Details.LocalToAbsolute(Details.GetLocalSize()).Y <= Stats.GetAbsolutePosition().Y + .1f);
			Test->TestTrue(TEXT("The stat summary fits above the footer"), Stats.LocalToAbsolute(Stats.GetLocalSize()).Y <= Footer.GetAbsolutePosition().Y + .1f);
		};
		CheckLayout(FVector2D(1920, 1080));
		Render(TEXT("Compact.png"), FVector2D(960, 540));
		CheckLayout(FVector2D(960, 540));

		// 긴 설명 데이터는 테스트 메모리에만 만들고 저장하지 않는다.
		TStrongObjectPtr<UCoreDataAsset> LongCore(NewObject<UCoreDataAsset>());
		LongCore->CoreId = TEXT("Test_LongDescription");
		LongCore->CoreName = FText::FromString(TEXT("복합 기능 시험 코어"));
		LongCore->MaxLevel = 2;
		FString Lines;
		for (int32 Line = 0; Line < 20; ++Line) Lines += FString::Printf(TEXT("효과 %d: 긴 조건과 추가 효과를 표시합니다.\n"), Line + 1);
		LongCore->CoreDescription = FText::FromString(Lines);
		Test->TestTrue(TEXT("Long description fixture can be granted"), IsCoreAcquireSuccess(Core->AcquireCore(LongCore.Get())));
		UWrapBox* StatBox = CastChecked<UWrapBox>(Screen->GetWidgetFromName(TEXT("StatCoresBox")));
		UUserWidget* Item = CastChecked<UUserWidget>(StatBox->GetChildAt(1));
		CastChecked<UButton>(Item->GetWidgetFromName(TEXT("CoreButton")))->OnClicked.Broadcast();
		CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("AbilityTab")))->OnClicked.Broadcast();
		CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("StatTab")))->OnClicked.Broadcast();
		Test->TestEqual(TEXT("Returning to a tab restores its selected core"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("CoreName")))->GetText().ToString(), LongCore->CoreName.ToString());
		Render(TEXT("LongEffect.png"), FVector2D(1920, 1080));
		UScrollBox* Effect = CastChecked<UScrollBox>(Screen->GetWidgetFromName(TEXT("EffectScroll")));
		Test->TestTrue(TEXT("Long effects have a real scroll extent"), Effect->GetScrollOffsetOfEnd() > 0);
		const auto Bottom = [](UWidget* W) { return W->GetCachedGeometry().LocalToAbsolute(W->GetCachedGeometry().GetLocalSize()).Y; };
		UWidget* Detail = Screen->GetWidgetFromName(TEXT("DetailPanel"));
		UWidget* Stats = Screen->GetWidgetFromName(TEXT("StatsRow"));
		Test->TestTrue(TEXT("Long effects stay above the stat summary"), Bottom(Detail) <= Stats->GetCachedGeometry().GetAbsolutePosition().Y);
		Test->TestTrue(TEXT("Drawing retains its design height"), Screen->GetWidgetFromName(TEXT("DrawingMinimum"))->GetCachedGeometry().GetLocalSize().Y >= 399);
		Effect->SetScrollOffset(200);
		Render(TEXT("LongEffectScrolled.png"), FVector2D(1920, 1080));
		Test->TestTrue(TEXT("Overflow text can actually be scrolled"), Effect->GetScrollOffset() > 0);
		Render(TEXT("Small.png"), FVector2D(1280, 720));
		CheckLayout(FVector2D(1280, 720));
		Test->TestTrue(TEXT("Scaled menu still fits vertically"), Bottom(Detail) <= Stats->GetCachedGeometry().GetAbsolutePosition().Y + .1);
		Render(TEXT("Ultrawide.png"), FVector2D(2560, 1080));
		CheckLayout(FVector2D(2560, 1080));
		Test->TestTrue(TEXT("Long core can reach its cap"), Core->AcquireCore(LongCore.Get()) == ECoreAcquireResult::Upgraded);
		Test->TestTrue(TEXT("Capped core displays no nonexistent next upgrade"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("NextEffect")))->GetText().ToString().Contains(TEXT("최대 단계")));
		const FKeyEvent IKey(EKeys::I, FModifierKeysState(), 0, false, 0, 0);
		Test->TestTrue(TEXT("I key is handled by the menu"), Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), IKey).IsEventHandled());
		Test->TestFalse(TEXT("I closes the menu"), Menu->IsViewerOpen());
		Test->TestFalse(TEXT("Closing resumes gameplay"), World->IsPaused());
		Test->TestEqual(TEXT("Closing restores the prior cursor visibility"), PC->bShowMouseCursor, PreviousCursor);
		Test->TestFalse(TEXT("GameAndUI retains its key-flush policy with a custom capture mode"), PC->ShouldFlushKeysWhenViewportFocusChanges());
		Test->TestTrue(TEXT("Closing restores the custom permanent capture mode"), Viewport->GetMouseCaptureMode() == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
		Test->TestTrue(TEXT("Closing restores the mouse lock mode"), Viewport->GetMouseLockMode() == EMouseLockMode::LockAlways);
		Test->TestFalse(TEXT("Closing restores hide-cursor-during-capture"), Viewport->HideCursorDuringCapture());
		Test->TestFalse(TEXT("Closing enables the original game input"), Viewport->IgnoreInput());
		Test->TestTrue(TEXT("Closing requests the original focus target"), LocalPlayer->GetSlateOperations().GetUserFocusRecepient() == PreviousFocus);

		PC->SetInputMode(FInputModeGameOnly());
		Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
		Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
		Viewport->SetHideCursorDuringCapture(true);
		Test->TestTrue(TEXT("Viewer opens from GameOnly with temporary mouse capture"), Menu->OpenViewer());
		Menu->CloseViewer();
		Test->TestTrue(TEXT("GameOnly retains its key-flush policy with a custom capture mode"), PC->ShouldFlushKeysWhenViewportFocusChanges());
		Test->TestTrue(TEXT("GameOnly restores its custom temporary capture mode"), Viewport->GetMouseCaptureMode() == EMouseCaptureMode::CaptureDuringMouseDown);
		Test->TestTrue(TEXT("GameOnly restores its cursor-capture setting"), Viewport->HideCursorDuringCapture());
		PC->SetInputMode(PriorMode);
		Test->TestTrue(TEXT("Menu can reopen"), Menu->OpenViewer());
		Screen = FindScreen(PC);
		if (!Screen) return true;
		Core->ResetOwnedCores();
		Test->TestEqual(TEXT("Reset removes owned stat icons"), CastChecked<UWrapBox>(Screen->GetWidgetFromName(TEXT("StatCoresBox")))->GetChildrenCount(), 0);
		Test->TestEqual(TEXT("Reset removes owned ability icons"), CastChecked<UWrapBox>(Screen->GetWidgetFromName(TEXT("AbilityCoresBox")))->GetChildrenCount(), 0);
		Test->TestTrue(TEXT("Reset keeps the full outer rectangle visible"), Screen->GetWidgetFromName(TEXT("DetailPanel"))->IsVisible());
		Test->TestEqual(TEXT("Reset refreshes the max health summary"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("MaxHealthValue")))->GetText().ToString(), FString(TEXT("100")));
		Test->TestFalse(TEXT("Reset hides the previous icon"), Screen->GetWidgetFromName(TEXT("DetailIcon"))->IsVisible());
		Test->TestFalse(TEXT("Reset hides the previous description"), Screen->GetWidgetFromName(TEXT("CoreDetails"))->IsVisible());
		for (const TCHAR* Name : {TEXT("CoreName"), TEXT("CoreLevel"), TEXT("CurrentEffect"), TEXT("NextEffect")})
			Test->TestTrue(FString::Printf(TEXT("Reset clears %s"), Name), CastChecked<UTextBlock>(Screen->GetWidgetFromName(Name))->GetText().IsEmpty());
		Render(TEXT("Empty.png"), FVector2D(1280, 720));
		CheckLayout(FVector2D(1280, 720));
		CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("StatTab")))->OnClicked.Broadcast();
		Test->TestTrue(TEXT("Empty tabs still switch after reset"), Screen->GetWidgetFromName(TEXT("StatTabIndicator"))->IsVisible());
		const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
		Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Escape);
		Test->TestFalse(TEXT("Escape also closes the viewer"), Menu->IsViewerOpen());
		Test->TestTrue(TEXT("Menu can open before forced removal"), Menu->OpenViewer());
		if (UOwnedCoreWidget* Forced = FindScreen(PC)) Forced->RemoveFromParent();
		Test->TestFalse(TEXT("External removal also releases the menu pause"), World->IsPaused());
		FInputModeUIOnly OtherUI;
		OtherUI.SetWidgetToFocus(PreviousFocus);
		PC->SetInputMode(OtherUI);
		Test->TestFalse(TEXT("An existing UIOnly screen prevents opening"), Menu->OpenViewer());
		Test->TestTrue(TEXT("Rejected opening preserves UIOnly input"), Viewport->IgnoreInput());
		PC->SetInputMode(PriorMode);
		PC->SetPause(true);
		Test->TestFalse(TEXT("Another pause screen prevents this menu from opening"), Menu->OpenViewer());
		Test->TestTrue(TEXT("An existing pause is preserved"), World->IsPaused());
		PC->SetPause(false);
		Test->TestTrue(TEXT("Viewer opens before another caller pauses"), Menu->OpenViewer());
		bool bOtherCallerCanUnpause = false;
		// 이미 멈춘 월드에 별도 해제 조건을 등록할 때는 GameMode의 일시정지 목록을 사용한다.
		Test->TestTrue(TEXT("Another caller registers its own pause condition"), World->GetAuthGameMode()->SetPause(PC,
			FCanUnpause::CreateLambda([&bOtherCallerCanUnpause]() { return bOtherCallerCanUnpause; })));
		Menu->CloseViewer();
		Test->TestTrue(TEXT("Closing preserves another caller's guarded pause"), World->IsPaused());
		bOtherCallerCanUnpause = true;
		PC->SetPause(false);
		Test->TestTrue(TEXT("Viewer opens before a pawn change"), Menu->OpenViewer());
		APawn* Replacement = World->SpawnActor<APawn>();
		PC->Possess(Replacement);
		Test->TestFalse(TEXT("Pawn replacement closes the old owned menu"), Menu->IsViewerOpen());
		Test->TestFalse(TEXT("Pawn replacement leaves no owned-menu pause"), World->IsPaused());
		return true;
	}
private:
	UOwnedCoreWidget* FindScreen(APlayerController* PC)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Widgets, UOwnedCoreWidget::StaticClass(), true);
		for (UUserWidget* W : Widgets) if (W->GetOwningPlayer() == PC && W->IsInViewport()) return Cast<UOwnedCoreWidget>(W);
		return nullptr;
	}
	FAutomationTestBase* Test;
	double Deadline;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedCoreMenuTest, "TheLastMessage.UI.OwnedCoreLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOwnedCoreMenuTest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld) { AddError(TEXT("End the current PIE session before running this test")); return false; }
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FOwnedCoreLifecycleCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
