#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Core/CoreComponent.h"
#include "Core/CorePickup.h"
#include "DataAsset/CoreDataAsset.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UI/CoreChoiceCardWidget.h"
#include "UI/CoreChoiceWidget.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

namespace
{
	/** 보상 예외를 재현하는 동안만 메모리의 DA 설정을 바꾸고, 범위를 벗어나면 복구한다. */
	struct FCorePoolOverride
	{
		struct FEntry
		{
			TStrongObjectPtr<UCoreDataAsset> Data;
			bool bIncluded;
			ECoreUpgradeLimitType Limit;
			int32 MaxLevel;
		};
		TArray<FEntry> Entries;
		explicit FCorePoolOverride(bool bInvalidateGrants)
		{
			TArray<FPrimaryAssetId> Ids;
			UAssetManager& Manager = UAssetManager::Get();
			Manager.GetPrimaryAssetIdList(UCoreDataAsset::StaticClass()->GetFName(), Ids);
			for (const FPrimaryAssetId& Id : Ids)
			{
				UCoreDataAsset* Data = Cast<UCoreDataAsset>(Manager.GetPrimaryAssetPath(Id).TryLoad());
				if (!Data) continue;
				Entries.Add({TStrongObjectPtr<UCoreDataAsset>(Data), Data->bIncludeInRandomPool, Data->UpgradeLimitType, Data->MaxLevel});
				if (bInvalidateGrants) { Data->UpgradeLimitType = ECoreUpgradeLimitType::Custom; Data->MaxLevel = 0; }
				else Data->bIncludeInRandomPool = false;
			}
		}
		~FCorePoolOverride()
		{
			for (FEntry& Entry : Entries)
			{
				Entry.Data->bIncludeInRandomPool = Entry.bIncluded;
				Entry.Data->UpgradeLimitType = Entry.Limit;
				Entry.Data->MaxLevel = Entry.MaxLevel;
			}
		}
	};

	/** 실제 선택 화면의 일시정지·페이드·강제 종료를 확인한다. 테스트 맵과 에셋은 저장하지 않는다. */
	class FCoreChoiceLifecycleCommand : public IAutomationLatentCommand
	{
	public:
		explicit FCoreChoiceLifecycleCommand(FAutomationTestBase* InTest)
			: Test(InTest), Deadline(FPlatformTime::Seconds() + 10.0) {}

		virtual bool Update() override
		{
			UWorld* World = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
			}
			if (!World)
			{
				if (FPlatformTime::Seconds() < Deadline) return false;
				Test->AddError(TEXT("PIE world is unavailable"));
				return true;
			}
			if (Phase == 0)
			{
				Controller = UGameplayStatics::GetPlayerController(World, 0);
				if (!Controller.IsValid() && FPlatformTime::Seconds() < Deadline) return false;
				if (!Test->TestNotNull(TEXT("Local controller exists"), Controller.Get())) return true;
				// 프로젝트의 기본 맵·게임 모드 설정과 분리해 코어 선택 흐름만 검사한다.
				Player = World->SpawnActor<APlayerBase>(FVector(0, 0, 1000), FRotator::ZeroRotator);
				if (!Test->TestNotNull(TEXT("PIE player exists"), Player.Get())) return true;
				APawn* PreviousPawn = Controller->GetPawn();
				Controller->Possess(Player.Get());
				if (PreviousPawn) PreviousPawn->Destroy();
				PreviousCursor = Controller->bShowMouseCursor;
				UClass* ScreenClass = LoadClass<UCoreChoiceWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice.WBP_CoreChoice_C"));
				Pickup = World->SpawnActor<ACorePickup>(FVector(10000, 0, 10000), FRotator::ZeroRotator);
				if (!Test->TestNotNull(TEXT("Choice screen BP exists"), ScreenClass) || !Test->TestNotNull(TEXT("Pickup exists"), Pickup.Get())) return true;
				FEnumProperty* Mode = FindFProperty<FEnumProperty>(ACorePickup::StaticClass(), TEXT("RewardMode"));
				Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Pickup.Get()), static_cast<int64>(ECoreRewardMode::Choice));
				FindFProperty<FClassProperty>(ACorePickup::StaticClass(), TEXT("ChoiceWidgetClass"))->SetObjectPropertyValue_InContainer(Pickup.Get(), ScreenClass);

				// 코어가 먼저 빨려 들어가고, 도착한 뒤에 선택 화면이 열린다.
				Test->TestTrue(TEXT("Pickup starts its absorb effect"), Pickup->TryOpenCoreChoice(Player.Get()));
				Test->TestNull(TEXT("Choice screen waits for the absorb effect"), FindScreen(World));
				Test->TestFalse(TEXT("Gameplay is not paused during the absorb effect"), UGameplayStatics::IsGamePaused(World));
				Advance(10);
				return false;
			}
			if (Phase == 10)
			{
				Screen = FindScreen(World);
				if (!Screen.IsValid())
				{
					if (FPlatformTime::Seconds() < Deadline) return false;
					Test->AddError(TEXT("Choice screen did not open after the absorb effect"));
					return true;
				}
				const FBoolProperty* Consumed = FindFProperty<FBoolProperty>(ACorePickup::StaticClass(), TEXT("bConsumed"));
				Test->TestTrue(TEXT("Absorbed pickup stays hidden while choosing"), Pickup.IsValid() && Pickup->IsHidden());
				Test->TestFalse(TEXT("Absorbed pickup is not consumed before selection"), Pickup.IsValid() && Consumed->GetPropertyValue_InContainer(Pickup.Get()));
				Test->TestTrue(TEXT("Game is paused while choosing"), UGameplayStatics::IsGamePaused(World));
				Test->TestNull(TEXT("Cancel button is removed from the WBP"), Screen->GetWidgetFromName(TEXT("CancelButton")));
				Test->TestTrue(TEXT("Mouse cursor is visible during selection"), Controller->bShowMouseCursor);
				const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
				Test->TestTrue(TEXT("ESC is consumed"), Screen->TakeWidget()->OnPreviewKeyDown(FGeometry(), Escape).IsEventHandled());
				Test->TestTrue(TEXT("ESC leaves the selection open"), Screen->IsInViewport());
				UGameplayStatics::SetGamePaused(World, false);
				Test->TestTrue(TEXT("Selection prevents an early unpause"), UGameplayStatics::IsGamePaused(World));
				UButton* Reroll = Cast<UButton>(Screen->GetWidgetFromName(TEXT("FirstRerollButton")));
				if (Test->TestNotNull(TEXT("First reroll button exists"), Reroll) && Reroll->GetIsEnabled())
				{
					Reroll->OnClicked.Broadcast();
					Test->TestFalse(TEXT("Reroll is still limited to one use"), Reroll->GetIsEnabled());
				}
				UHorizontalBox* Cards = Cast<UHorizontalBox>(Screen->GetWidgetFromName(TEXT("CardsBox")));
				UCoreChoiceCardWidget* Card = Cards ? Cast<UCoreChoiceCardWidget>(Cards->GetChildAt(0)) : nullptr;
				UButton* Choose = Card ? Cast<UButton>(Card->GetWidgetFromName(TEXT("CardButton"))) : nullptr;
				if (!Test->TestNotNull(TEXT("First choice card button exists"), Choose)) return true;
				Choose->OnClicked.Broadcast();
				Test->TestTrue(TEXT("Successful selection consumes its pickup"), !Pickup.IsValid() || Pickup->IsActorBeingDestroyed());
				Test->TestTrue(TEXT("Game stays paused during the close fade"), UGameplayStatics::IsGamePaused(World));
				Advance(1);
				return false;
			}

			// 시간 제한은 게임 시간이 아니라 실제 시간으로 계산한다. 선택 중 게임 시간은 멈춰 있다.
			const bool bScreenGone = !Screen.IsValid() || !Screen->IsInViewport();
			const bool bPaused = UGameplayStatics::IsGamePaused(World);
			if (!bScreenGone || (Phase != 5 && bPaused))
			{
				if (FPlatformTime::Seconds() < Deadline) return false;
				Test->AddError(TEXT("Selection did not close or release its pause"));
				if (Screen.IsValid()) Screen->CloseChoiceUI(false);
				return true;
			}
			Test->TestEqual(TEXT("Original cursor visibility is restored"), Controller->bShowMouseCursor, PreviousCursor);
			if (Phase == 1)
			{
				// 새 픽업으로 외부 화면 제거와 기존 일시정지 보존을 검사한다. 이후 단계는 흡수 연출 없이 바로 연다.
				Pickup = World->SpawnActor<ACorePickup>(FVector(11000, 0, 10000), FRotator::ZeroRotator);
				FEnumProperty* Mode = FindFProperty<FEnumProperty>(ACorePickup::StaticClass(), TEXT("RewardMode"));
				Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Pickup.Get()), static_cast<int64>(ECoreRewardMode::Choice));
				FindFProperty<FClassProperty>(ACorePickup::StaticClass(), TEXT("ChoiceWidgetClass"))->SetObjectPropertyValue_InContainer(Pickup.Get(), LoadClass<UCoreChoiceWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice.WBP_CoreChoice_C")));
				FindFProperty<FFloatProperty>(ACorePickup::StaticClass(), TEXT("AbsorbDuration"))->SetPropertyValue_InContainer(Pickup.Get(), 0.0f);
				if (!OpenScreen(World)) return true;
				Screen->RemoveFromParent();
				Advance(2);
				return false;
			}
			if (Phase == 2)
			{
				{
					FCorePoolOverride EmptyPool(false);
					Test->TestFalse(TEXT("An empty pool never opens its screen"), Pickup->TryOpenCoreChoice(Player.Get()));
					TArray<UUserWidget*> Screens;
					UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, UCoreChoiceWidget::StaticClass(), true);
					Test->TestTrue(TEXT("No choice UI exists when there are no candidates"), Screens.IsEmpty());
					Test->TestFalse(TEXT("An empty pool does not pause gameplay"), UGameplayStatics::IsGamePaused(World));
					Test->TestTrue(TEXT("An empty pool keeps its pickup"), Pickup.IsValid() && !Pickup->IsActorBeingDestroyed());
					Test->TestFalse(TEXT("A second attempt also keeps the screen closed"), Pickup->TryOpenCoreChoice(Player.Get()));
				}
				if (!OpenScreen(World)) return true;
				Player->GetCoreComponent()->ResetOwnedCores();
				Advance(3);
				return false;
			}
			if (Phase == 3)
			{
				if (!OpenScreen(World)) return true;
				{
					FCorePoolOverride InvalidGrants(true);
					UHorizontalBox* Cards = Cast<UHorizontalBox>(Screen->GetWidgetFromName(TEXT("CardsBox")));
					UCoreChoiceCardWidget* Card = Cards ? Cast<UCoreChoiceCardWidget>(Cards->GetChildAt(0)) : nullptr;
					UButton* Choose = Card ? Cast<UButton>(Card->GetWidgetFromName(TEXT("CardButton"))) : nullptr;
					if (!Test->TestNotNull(TEXT("Failed selection still has a click target"), Choose)) return true;
					Choose->OnClicked.Broadcast();
				}
				Test->TestTrue(TEXT("An ungrantable reward does not consume its pickup"), Pickup.IsValid() && !Pickup->IsActorBeingDestroyed());
				Advance(4);
				return false;
			}
			if (Phase == 4)
			{
				Test->TestTrue(TEXT("External pause begins"), UGameplayStatics::SetGamePaused(World, true));
				if (!OpenScreen(World)) return true;
				Screen->CloseChoiceUI(false);
				Advance(5);
				return false;
			}
			if (Phase == 5)
			{
				Test->TestTrue(TEXT("Pause owned by another caller is preserved"), bPaused);
				UGameplayStatics::SetGamePaused(World, false);
				if (!OpenScreen(World)) return true;
				Player->HandleDeath();
				Advance(6);
				return false;
			}
			return true;
		}

	private:
		bool OpenScreen(UWorld* World)
		{
			if (!Test->TestTrue(TEXT("Pickup opens a choice screen"), Pickup->TryOpenCoreChoice(Player.Get()))) return false;
			Screen = FindScreen(World);
			if (!Test->TestNotNull(TEXT("Choice screen is in the viewport"), Screen.Get())) return false;
			Test->TestTrue(TEXT("Game is paused while choosing"), UGameplayStatics::IsGamePaused(World));
			return true;
		}
		static UCoreChoiceWidget* FindScreen(UWorld* World)
		{
			TArray<UUserWidget*> Screens;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, UCoreChoiceWidget::StaticClass(), true);
			return Screens.IsEmpty() ? nullptr : Cast<UCoreChoiceWidget>(Screens[0]);
		}
		void Advance(int32 NextPhase) { Phase = NextPhase; Deadline = FPlatformTime::Seconds() + 4.0; }
		FAutomationTestBase* Test;
		TWeakObjectPtr<APlayerBase> Player;
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<ACorePickup> Pickup;
		TWeakObjectPtr<UCoreChoiceWidget> Screen;
		int32 Phase = 0;
		double Deadline = 0;
		bool PreviousCursor = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreChoiceLifecycleTest, "TheLastMessage.UI.CoreChoiceLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreChoiceLifecycleTest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld) { AddError(TEXT("End the current PIE session before running this test")); return false; }
	UWorld* EditorWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!EditorWorld) { AddError(TEXT("Could not create an unsaved test map")); return false; }
	EditorWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FCoreChoiceLifecycleCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
