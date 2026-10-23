#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/Abilities/GA_AttackPrimary.h"
#include "GAS/WeaponGameplayTags.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GameplayEffect.h"
#include "Projectile/ProjectileBase.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SphereComponent.h"
#include "Components/TextBlock.h"
#include "Core/CoreComponent.h"
#include "Core/CorePickup.h"
#include "DataAsset/CoreDataAsset.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "TextureCompiler.h"
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
				FInputModeGameAndUI PriorMode;
				PriorMode.SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Controller->SetInputMode(PriorMode);
				World->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
				PreviousCursor = Controller->bShowMouseCursor;
				UClass* ScreenClass = LoadClass<UCoreChoiceWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice.WBP_CoreChoice_C"));
				Pickup = World->SpawnActor<ACorePickup>(FVector(10000, 0, 10000), FRotator::ZeroRotator);
				if (!Test->TestNotNull(TEXT("Choice screen BP exists"), ScreenClass) || !Test->TestNotNull(TEXT("Pickup exists"), Pickup.Get())) return true;
				FEnumProperty* Mode = FindFProperty<FEnumProperty>(ACorePickup::StaticClass(), TEXT("RewardMode"));
				Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Pickup.Get()), static_cast<int64>(ECoreRewardMode::Choice));
				FindFProperty<FClassProperty>(ACorePickup::StaticClass(), TEXT("ChoiceWidgetClass"))->SetObjectPropertyValue_InContainer(Pickup.Get(), ScreenClass);

				// 차지 중 보상창이 열려 release가 UI로 전달되는 상황을 재현한다.
				UWeaponDataAsset* Weapon = NewObject<UWeaponDataAsset>(Player.Get());
				Weapon->WeaponType = EWeaponType::LightRanged;
				Weapon->AttackPower = 10.0f;
				Weapon->Range = 1000.0f;
				Weapon->ProjectileSpeed = 1000.0f;
				Weapon->ProjectileClass = AProjectileBase::StaticClass();
				Weapon->EnergyPerShot = 0.0f;
				Weapon->bSupportsChargeAttack = true;
				Weapon->MaxChargeTime = 1.0f;
				Weapon->ChargeStartDelay = 0.0f;
				FindFProperty<FClassProperty>(APlayerBase::StaticClass(), TEXT("WeaponDamageEffectClass"))
					->SetObjectPropertyValue_InContainer(Player.Get(), UGameplayEffect::StaticClass());
				Test->TestTrue(TEXT("Equip charge weapon before core selection"), Player->EquipWeapon(EWeaponSlot::Primary, Weapon));
				UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
				FGameplayAbilitySpec* Attack = ASC->FindAbilitySpecFromClass(UGA_AttackPrimary::StaticClass());
				if (!Test->TestNotNull(TEXT("Primary attack exists"), Attack)) return true;
				Attack->InputPressed = true;
				Test->TestTrue(TEXT("Charge starts before core selection"), ASC->TryActivateAbility(Attack->Handle));
				Test->TestTrue(TEXT("Charge is active before core selection"), Player->IsChargingWeapon());
				Player->SetWeaponChoiceHeld(0, true);
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
				Test->TestFalse(TEXT("Core selection cancels charge"), Player->IsChargingWeapon());
				Test->TestFalse(TEXT("Core selection clears attack lock"), Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking));
				Test->TestEqual(TEXT("Core selection clears held weapon choice"), Player->GetHeldWeaponChoice(), INDEX_NONE);
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
				if (Test->TestNotNull(TEXT("First reroll button exists"), Reroll)
					&& Test->TestTrue(TEXT("A replacement is available for the hint test"), Reroll->GetIsEnabled()))
				{
					UTextBlock* Hint = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("HintText")));
					if (!Test->TestNotNull(TEXT("Status hint exists"), Hint)) return true;
					const FString DefaultHint = Hint->GetText().ToString();
					const FText Failure = FText::FromString(TEXT("Test: choice could not be granted"));
					Screen->SetStatusMessage(Failure);
					Reroll->OnClicked.Broadcast();
					Test->TestFalse(TEXT("Reroll is still limited to one use"), Reroll->GetIsEnabled());
					Test->TestEqual(TEXT("Successful reroll restores the designer hint"), Hint->GetText().ToString(), DefaultHint);
					Screen->SetStatusMessage(Failure);
					// 사용한 카드에 다시 요청해도 후보와 오류 안내를 바꾸지 않는다.
					Screen->OnRerollRequested.Broadcast(0);
					Test->TestEqual(TEXT("Rejected reroll preserves the failure hint"), Hint->GetText().ToString(), Failure.ToString());
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
			Test->TestEqual(TEXT("Choice close restores the original key-flush policy"), Controller->ShouldFlushKeysWhenViewportFocusChanges(), Phase == 5);
			Test->TestEqual(TEXT("Choice close restores whether game input was blocked"), World->GetGameViewport()->IgnoreInput(), Phase == 5);
			Test->TestTrue(TEXT("Choice close restores custom mouse capture"), World->GetGameViewport()->GetMouseCaptureMode()
				== (Phase == 5 ? EMouseCaptureMode::NoCapture : EMouseCaptureMode::CaptureDuringRightMouseDown));
			Test->TestFalse(TEXT("Choice close restores cursor capture visibility"), World->GetGameViewport()->HideCursorDuringCapture());
			Test->TestTrue(TEXT("Choice close restores mouse lock"), World->GetGameViewport()->GetMouseLockMode() == EMouseLockMode::DoNotLock);
			if (Phase == 1)
			{
				// 새 픽업으로 외부 화면 제거와 기존 일시정지 보존을 검사한다. 이후 단계는 흡수 연출 없이 바로 연다.
				const FTransform Placement(FRotator::ZeroRotator, FVector(11000, 0, 10000), FVector(1.25, .75, 1.5));
				Pickup = World->SpawnActorDeferred<ACorePickup>(ACorePickup::StaticClass(), Placement);
				FEnumProperty* Mode = FindFProperty<FEnumProperty>(ACorePickup::StaticClass(), TEXT("RewardMode"));
				Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Pickup.Get()), static_cast<int64>(ECoreRewardMode::Choice));
				FindFProperty<FClassProperty>(ACorePickup::StaticClass(), TEXT("ChoiceWidgetClass"))->SetObjectPropertyValue_InContainer(Pickup.Get(), LoadClass<UCoreChoiceWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice.WBP_CoreChoice_C")));
				FindFProperty<FFloatProperty>(ACorePickup::StaticClass(), TEXT("AbsorbDuration"))->SetPropertyValue_InContainer(Pickup.Get(), 0.0f);
				// 복구 직후와 다음 프레임의 배치값을 비교할 때 대기 연출의 변화를 제외한다.
				for (const TCHAR* Property : {TEXT("BobHeight"), TEXT("PulseScale"), TEXT("SpinSpeed")})
					FindFProperty<FFloatProperty>(ACorePickup::StaticClass(), Property)->SetPropertyValue_InContainer(Pickup.Get(), 0.f);
				Pickup->FinishSpawning(Placement);
				if (!OpenScreen(World)) return true;
				Screen->RemoveFromParent();
				Advance(2);
				return false;
			}
			if (Phase == 2)
			{
				CheckRestored(TEXT("External screen removal"));
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
				CheckRestored(TEXT("Reset while choosing"));
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
				CheckRestored(TEXT("Exhausted choices after a failed selection"));
				Test->TestTrue(TEXT("External pause begins"), UGameplayStatics::SetGamePaused(World, true));
				FInputModeUIOnly OtherUI;
				OtherUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				Controller->SetInputMode(OtherUI);
				if (!OpenScreen(World)) return true;
				Screen->CloseChoiceUI(false);
				Advance(5);
				return false;
			}
			if (Phase == 5)
			{
				Test->TestTrue(TEXT("Pause owned by another caller is preserved"), bPaused);
				FInputModeGameAndUI PriorMode;
				PriorMode.SetHideCursorDuringCapture(false);
				Controller->SetInputMode(PriorMode);
				World->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
				UGameplayStatics::SetGamePaused(World, false);
				if (!OpenScreen(World)) return true;
				Player->HandleDeath();
				Advance(6);
				return false;
			}
			if (Phase == 6)
			{
				CheckRestored(TEXT("Death while choosing"));
				if (!ReplacePlayer(World)) return true;
				const FFloatProperty* Duration = FindFProperty<FFloatProperty>(ACorePickup::StaticClass(), TEXT("AbsorbDuration"));
				Duration->SetPropertyValue_InContainer(Pickup.Get(), .4f);
				Test->TestTrue(TEXT("Choice absorption begins before death"), Pickup->TryOpenCoreChoice(Player.Get()));
				Pickup->Tick(.2f);
				Test->TestFalse(TEXT("Absorption moves the pickup away from its placed position"),
					Pickup->GetActorLocation().Equals(FVector(11000, 0, 10000)));
				Test->TestTrue(TEXT("Absorption disables pickup collision"),
					Pickup->FindComponentByClass<USphereComponent>()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
				Player->HandleDeath();
				CheckRestored(TEXT("Death during choice absorption"));
				Pickup->Tick(.5f);
				Test->TestTrue(TEXT("A restored pickup survives the old absorption deadline"),
					Pickup.IsValid() && !Pickup->IsActorBeingDestroyed());
				Test->TestNull(TEXT("Death during absorption never opens its choice screen"), FindScreen(World));

				if (!ReplacePlayer(World)) return true;
				Test->TestTrue(TEXT("Choice absorption begins before target destruction"), Pickup->TryOpenCoreChoice(Player.Get()));
				Pickup->Tick(.2f);
				Player->Destroy();
				CheckRestored(TEXT("Target destruction during absorption"));
				if (!ReplacePlayer(World)) return true;

				// 카드 배치가 없는 기본 클래스로 화면 생성 실패를 재현한다.
				const FClassProperty* ScreenProperty = FindFProperty<FClassProperty>(ACorePickup::StaticClass(), TEXT("ChoiceWidgetClass"));
				ScreenProperty->SetObjectPropertyValue_InContainer(Pickup.Get(), UCoreChoiceWidget::StaticClass());
				Test->TestTrue(TEXT("Candidates can be reserved before an unbuildable screen"), Pickup->TryOpenCoreChoice(Player.Get()));
				Pickup->Tick(.5f);
				CheckRestored(TEXT("Screen creation failure after absorption"));
				Test->TestNull(TEXT("Failed screen creation leaves no visible screen"), FindScreen(World));
				Test->TestFalse(TEXT("Failed screen creation leaves gameplay unpaused"), UGameplayStatics::IsGamePaused(World));
				ScreenProperty->SetObjectPropertyValue_InContainer(Pickup.Get(),
					LoadClass<UCoreChoiceWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice.WBP_CoreChoice_C")));
				Test->TestTrue(TEXT("A failed screen releases the reward reservation for retry"), Pickup->TryOpenCoreChoice(Player.Get()));
				Player->GetCoreComponent()->ResetOwnedCores();
				CheckRestored(TEXT("Reset during choice absorption"));

				// 고정 보상은 흡수 시작 시 지급하므로 연출 종료나 사망으로 재지급되지 않아야 한다.
				UCoreDataAsset* FixedCore = NewObject<UCoreDataAsset>(Player.Get());
				FixedCore->CoreId = TEXT("Test_FixedPickupLifecycle");
				FixedCore->MaxLevel = 0;
				FEnumProperty* Mode = FindFProperty<FEnumProperty>(ACorePickup::StaticClass(), TEXT("RewardMode"));
				Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Pickup.Get()), static_cast<int64>(ECoreRewardMode::Fixed));
				const FObjectProperty* CoreProperty = FindFProperty<FObjectProperty>(ACorePickup::StaticClass(), TEXT("CoreData"));
				CoreProperty->SetObjectPropertyValue_InContainer(Pickup.Get(), FixedCore);
				Test->TestFalse(TEXT("An invalid fixed reward is rejected"), Pickup->TryAcquireFixedCore(Player->GetCoreComponent()));
				CheckRestored(TEXT("Failed fixed reward"));
				FixedCore->MaxLevel = 5;
				Test->TestTrue(TEXT("A corrected fixed reward is granted"), Pickup->TryAcquireFixedCore(Player->GetCoreComponent()));
				Test->TestEqual(TEXT("Fixed reward is granted before absorption completes"), Player->GetCoreComponent()->GetCoreLevel(FixedCore->CoreId), 1);
				Test->TestFalse(TEXT("A consumed pickup cannot grant twice during absorption"), Pickup->TryAcquireFixedCore(Player->GetCoreComponent()));
				Pickup->Tick(.5f);
				Test->TestTrue(TEXT("Fixed reward is removed after absorption"), !Pickup.IsValid() || Pickup->IsActorBeingDestroyed());
				Test->TestEqual(TEXT("Finishing absorption does not grant again"), Player->GetCoreComponent()->GetCoreLevel(FixedCore->CoreId), 1);
				for (const float AbsorbSeconds : {0.f, .4f})
				{
					Pickup = World->SpawnActor<ACorePickup>(FVector(12000, 0, 10000), FRotator::ZeroRotator);
					Duration->SetPropertyValue_InContainer(Pickup.Get(), AbsorbSeconds);
					CoreProperty->SetObjectPropertyValue_InContainer(Pickup.Get(), FixedCore);
					Test->TestTrue(TEXT("Another fixed pickup upgrades the same core once"), Pickup->TryAcquireFixedCore(Player->GetCoreComponent()));
					if (AbsorbSeconds > 0.f)
					{
						Player->HandleDeath();
						Pickup->Tick(.5f);
					}
					Test->TestTrue(TEXT("A fixed reward with no animation or a dead target is removed"), !Pickup.IsValid() || Pickup->IsActorBeingDestroyed());
				}
				Test->TestEqual(TEXT("Three fixed pickups grant exactly three levels, including the one interrupted by death"),
					Player->GetCoreComponent()->GetCoreLevel(FixedCore->CoreId), 3);
				Test->TestNull(TEXT("Fixed rewards never open the choice screen"), FindScreen(World));
				Test->TestFalse(TEXT("Pickup exception cases leave no owned pause"), UGameplayStatics::IsGamePaused(World));
			}
			return true;
		}

	private:
		void CheckRestored(const TCHAR* Case)
		{
			Test->AddInfo(FString::Printf(TEXT("Pickup restoration: %s"), Case));
			if (!Test->TestTrue(TEXT("Unconsumed pickup is still available"), Pickup.IsValid() && !Pickup->IsActorBeingDestroyed())) return;
			Test->TestFalse(TEXT("Restored pickup is visible"), Pickup->IsHidden());
			Test->TestTrue(TEXT("Restored pickup resumes its tick"), Pickup->IsActorTickEnabled());
			Test->TestTrue(TEXT("Restored pickup returns to its placed position"), Pickup->GetActorLocation().Equals(FVector(11000, 0, 10000), .01));
			Test->TestTrue(TEXT("Restored pickup retains its placed scale"), Pickup->GetActorScale3D().Equals(FVector(1.25, .75, 1.5), .01));
			Test->TestTrue(TEXT("Restored pickup enables overlap queries"),
				Pickup->FindComponentByClass<USphereComponent>()->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
			Test->TestFalse(TEXT("Failed or interrupted choices do not consume the pickup"),
				FindFProperty<FBoolProperty>(ACorePickup::StaticClass(), TEXT("bConsumed"))->GetPropertyValue_InContainer(Pickup.Get()));
			Test->TestNull(TEXT("Restored pickup has no active reward session"),
				FindFProperty<FObjectProperty>(ACorePickup::StaticClass(), TEXT("ActiveRewardSession"))->GetObjectPropertyValue_InContainer(Pickup.Get()));
		}
		bool ReplacePlayer(UWorld* World)
		{
			APlayerBase* Previous = Player.Get();
			Player = World->SpawnActor<APlayerBase>(FVector(0, 0, 1000), FRotator::ZeroRotator);
			if (!Test->TestNotNull(TEXT("A fresh player exists for the next exception case"), Player.Get())) return false;
			FindFProperty<FBoolProperty>(APlayerBase::StaticClass(), TEXT("bAutoRespawn"))->SetPropertyValue_InContainer(Player.Get(), false);
			Controller->Possess(Player.Get());
			if (IsValid(Previous)) Previous->Destroy();
			return true;
		}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoreCardLayoutTest, "TheLastMessage.UI.CoreCardLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoreCardLayoutTest::RunTest(const FString& Parameters)
{
	UClass* CardClass = LoadClass<UCoreChoiceCardWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoiceCard.WBP_CoreChoiceCard_C"));
	if (!TestNotNull(TEXT("Card WBP exists"), CardClass)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UCoreChoiceCardWidget* Card = CreateWidget<UCoreChoiceCardWidget>(World, CardClass);
	if (!TestNotNull(TEXT("Card can be created"), Card))
	{
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	UCoreDataAsset* Data = NewObject<UCoreDataAsset>(Card);
	Data->CoreName = FText::FromString(TEXT("보강 프레임"));
	Data->CoreDescription = FText::FromString(TEXT("기본 최대 체력 대비 +30% → +45%"));
	Data->MaxLevel = 5;
	Data->CoreIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Test_Jihun/Core/UI/T_UI_CoreIcon_Default.T_UI_CoreIcon_Default"));
	Card->SetCoreData(Data, 0, 2);
	TSharedRef<SWidget> SlateCard = Card->TakeWidget();
	UTextBlock* Name = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("CoreNameText")));
	UWidget* Level = Card->GetWidgetFromName(TEXT("CoreLevelText"));
	UWidget* Description = Card->GetWidgetFromName(TEXT("CoreDescriptionText"));
	UWidget* Divider = Card->GetWidgetFromName(TEXT("DividerSizeBox"));
	UWidget* Action = Card->GetWidgetFromName(TEXT("ActionBox"));
	if (TestNotNull(TEXT("Name exists"), Name) && TestNotNull(TEXT("Level exists"), Level)
		&& TestNotNull(TEXT("Description exists"), Description) && TestNotNull(TEXT("Divider exists"), Divider)
		&& TestNotNull(TEXT("Action exists"), Action))
	{
		// 실제 WBP를 그려 줄바꿈 이후의 배치와 고정된 하단 영역을 검사한다.
		TArray<UTexture*> Textures = {Data->CoreIcon};
		if (UImage* Pattern = Cast<UImage>(Card->GetWidgetFromName(TEXT("DiagramPatternImage"))))
		{
			if (UTexture* Texture = Cast<UTexture>(Pattern->GetBrush().GetResourceObject())) Textures.Add(Texture);
		}
		FTextureCompilingManager::Get().FinishCompilation(Textures);
		FlushRenderingCommands();
		FWidgetRenderer Renderer(true);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateCard, FVector2D(350, 510));
		if (TestNotNull(TEXT("Layout test requires rendering (do not use -NullRHI)"), Target))
		{
			auto Render = [&](const TCHAR* Filename)
			{
				for (int32 Frame = 0; Frame < 3; ++Frame)
				{
					++GFrameCounter;
					Renderer.DrawWidget(Target, SlateCard, FVector2D(350, 510), 0.016f);
					FlushRenderingCommands();
				}
				FBufferArchive PNG;
				const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/CoreCardLayout") / Filename;
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
				TestTrue(TEXT("Layout preview is exported"), FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG)
					&& FFileHelper::SaveArrayToFile(PNG, *Path));
			};
			auto Top = [](UWidget* Widget) { return Widget->GetCachedGeometry().GetAbsolutePosition().Y; };
			auto Bottom = [&](UWidget* Widget) { return Top(Widget) + Widget->GetCachedGeometry().GetLocalSize().Y; };
			Render(TEXT("Short.png"));
			const double LevelTop = Top(Level), DividerTop = Top(Divider), ActionTop = Top(Action);
			Data->CoreName = FText::FromString(TEXT("고성능 복합 장갑\n출력 증폭 장치"));
			Data->CoreDescription = FText::FromString(TEXT("체력이 30% 이하일 때 피해량이 30% → 40% 증가합니다. 긴 설명과 NoSpacesABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ를 함께 표시합니다."));
			Card->SetCoreData(Data, 0, 2);
			Render(TEXT("Long.png"));
			AddInfo(FString::Printf(TEXT("Two-line name: desired %.1f, allocated %.1f"), Name->GetDesiredSize().Y, Name->GetCachedGeometry().GetLocalSize().Y));
			TestTrue(TEXT("Two name lines fit the reserved height"), Name->GetDesiredSize().Y <= Name->GetCachedGeometry().GetLocalSize().Y + 1.0);
			Data->CoreName = FText::FromString(TEXT("고성능\n복합 장갑\n출력 증폭\n초장문 이름"));
			Card->SetCoreData(Data, 0, 2);
			Render(TEXT("Overflow.png"));
			TestTrue(TEXT("Name does not push level text"), FMath::IsNearlyEqual(Top(Level), LevelTop, 0.1));
			TestTrue(TEXT("Description does not push divider"), FMath::IsNearlyEqual(Top(Divider), DividerTop, 0.1));
			TestTrue(TEXT("Selection action stays at the same position"), FMath::IsNearlyEqual(Top(Action), ActionTop, 0.1));
			TestTrue(TEXT("Name and level do not overlap"), Bottom(Name) <= Top(Level) + 0.1);
			TestTrue(TEXT("Description stays above divider"), Bottom(Description) <= Top(Divider) + 0.1);
			TestTrue(TEXT("Action remains inside the card"), Bottom(Action) <= 510.0);

			UClass* ScreenClass = LoadClass<UCoreChoiceWidget>(nullptr, TEXT("/Game/Test_Jihun/Core/WBP/WBP_CoreChoice.WBP_CoreChoice_C"));
			UCoreChoiceWidget* Screen = ScreenClass ? CreateWidget<UCoreChoiceWidget>(World, ScreenClass) : nullptr;
			if (TestNotNull(TEXT("Screen WBP exists"), Screen))
			{
				UHorizontalBox* Cards = CastChecked<UHorizontalBox>(Screen->GetWidgetFromName(TEXT("CardsBox")));
				Cards->ClearChildren();
				for (int32 Index = 0; Index < 3; ++Index)
				{
					UCoreChoiceCardWidget* Preview = CreateWidget<UCoreChoiceCardWidget>(World, CardClass);
					Data->CoreName = FText::FromString(TEXT("고성능 복합 장갑\n출력 증폭 장치"));
					Preview->SetCoreData(Data, Index, 2);
					Cards->AddChildToHorizontalBox(Preview)->SetPadding(FMargin(12, 0));
				}
				TSharedRef<SWidget> SlateScreen = Screen->TakeWidget();
				UTextureRenderTarget2D* ScreenTarget = Renderer.DrawWidget(SlateScreen, FVector2D(1920, 1080));
				if (TestNotNull(TEXT("Screen renders"), ScreenTarget))
				{
					for (int32 Frame = 0; Frame < 3; ++Frame)
					{
						++GFrameCounter;
						Renderer.DrawWidget(ScreenTarget, SlateScreen, FVector2D(1920, 1080), 0.016f);
						FlushRenderingCommands();
					}
					UWidget* Layout = Screen->GetWidgetFromName(TEXT("LayoutSizeBox"));
					UWidget* Footer = Screen->GetWidgetFromName(TEXT("FooterBox"));
					const FVector2D FooterBottom = Footer->GetCachedGeometry().LocalToAbsolute(Footer->GetCachedGeometry().GetLocalSize());
					const FVector2D LayoutBottom = Layout->GetCachedGeometry().LocalToAbsolute(Layout->GetCachedGeometry().GetLocalSize());
					TestTrue(TEXT("Footer stays inside the screen layout"), FooterBottom.Y <= LayoutBottom.Y + 0.1);
					FBufferArchive PNG;
					TestTrue(TEXT("Screen preview is exported"), FImageUtils::ExportRenderTarget2DAsPNG(ScreenTarget, PNG)
						&& FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / TEXT("Automation/CoreCardLayout/Screen.png"))));
				}
				Screen->ReleaseSlateResources(true);
			}
		}
	}
	Card->ReleaseSlateResources(true);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
