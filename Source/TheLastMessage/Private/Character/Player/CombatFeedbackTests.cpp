#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/CombatFeedbackComponent.h"
#include "Character/Player/CombatHitStopSubsystem.h"
#include "Character/Player/CombatCameraModifier.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/CombatDamageLibrary.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFeedbackTest, "TheLastMessage.Combat.FeedbackAndHitStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatFeedbackTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	auto Advance = [World](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.01f);
		}
	};
	UCombatHitStopSubsystem* Stop = World->GetSubsystem<UCombatHitStopSubsystem>();
	AWorldSettings* Settings = World->GetWorldSettings();
	if (!TestNotNull(TEXT("World owns hit stop subsystem"), Stop)) return false;
	Settings->SetTimeDilation(0.5f);
	Stop->RequestHitStop(0.05f, 0.03f);
	TestTrue(TEXT("Hit stop slows existing time scale"), FMath::IsNearlyEqual(Settings->TimeDilation, 0.015f));
	Advance(2);
	Stop->RequestHitStop(0.08f, 0.03f);
	TestTrue(TEXT("Overlapping hits do not multiply slow motion"), FMath::IsNearlyEqual(Settings->TimeDilation, 0.015f));
	Advance(12);
	TestFalse(TEXT("Undilated time expires hit stop"), Stop->IsHitStopActive());
	TestEqual(TEXT("Previous slow motion is restored"), Settings->TimeDilation, 0.5f);
	Stop->RequestHitStop(0.05f, 0.03f);
	Settings->SetTimeDilation(0.7f);
	Advance(7);
	TestEqual(TEXT("External time change is preserved"), Settings->TimeDilation, 0.7f);
	Stop->RequestHitStop(0, 0.03f);
	TestFalse(TEXT("Zero duration disables stop"), Stop->IsHitStopActive());
	Settings->SetTimeDilation(1.0f);

	APlayerBase* Player = World->SpawnActor<APlayerBase>(FVector(0, 0, 200), FRotator::ZeroRotator);
	APlayerController* PC = World->SpawnActor<APlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine);
	PC->Possess(Player);
	Player->GetCharacterMovement()->DisableMovement();
	Player->GetCombatAttributes()->SetMaxHealth(100);
	Player->GetCombatAttributes()->SetHealth(100);
	UCombatFeedbackComponent* Feedback = Player->FindComponentByClass<UCombatFeedbackComponent>();
	TestNotNull(TEXT("Player inherits editable feedback component"), Feedback);
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>(FVector(500, 0, 200), FRotator::ZeroRotator);
	Enemy->GetCharacterMovement()->DisableMovement();
	UClass* DamageEffect = LoadClass<UGameplayEffect>(nullptr,
		TEXT("/Game/Test_Inhyeok/GAS/Effect/GE_BP_CommonIncomingDamage.GE_BP_CommonIncomingDamage_C"));
	TestTrue(TEXT("Real melee damage applies"), UCombatDamageLibrary::ApplyFinalDamageToTarget(
		Player, Enemy, DamageEffect, 10, WeaponGameplayTags::Damage_PlayerMelee, 1.5f));
	TestTrue(TEXT("Successful damage triggers hit stop"), Stop->IsHitStopActive());
	UCombatCameraModifier* Modifier = PC->PlayerCameraManager
		? Cast<UCombatCameraModifier>(PC->PlayerCameraManager->FindCameraModifierByClass(UCombatCameraModifier::StaticClass())) : nullptr;
	if (TestNotNull(TEXT("Owner camera receives combat modifier"), Modifier))
	{
		FMinimalViewInfo POV;
		Modifier->ModifyCamera(0.01f, POV);
		TestFalse(TEXT("Impact actually offsets camera"), POV.Location.IsNearlyZero());
		Advance(25);
		FMinimalViewInfo RestoredPOV;
		Modifier->ModifyCamera(0.01f, RestoredPOV);
		TestTrue(TEXT("Camera returns without residual offset"), RestoredPOV.Location.IsNearlyZero() && RestoredPOV.Rotation.IsNearlyZero());
	}
	Advance(25);
	Enemy->HandleDeath();
	UCombatDamageLibrary::ApplyFinalDamageToTarget(Player, Enemy, DamageEffect, 10, WeaponGameplayTags::Damage_PlayerRanged);
	TestFalse(TEXT("Accepted damage on dead enemy produces no false impact"), Stop->IsHitStopActive());
	UCombatDamageLibrary::ApplyFinalDamageToTarget(Player, Enemy, DamageEffect, 0, WeaponGameplayTags::Damage_PlayerMelee);
	TestFalse(TEXT("Zero damage produces no impact"), Stop->IsHitStopActive());

	auto ApplyPlayerHealth = [Player](float Amount)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>();
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Mod = Effect->Modifiers.AddDefaulted_GetRef();
		Mod.Attribute = UCombatAttributeSet::GetHealthAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		ASC->ApplyGameplayEffectToSelf(Effect, 1, ASC->MakeEffectContext());
	};
	ApplyPlayerHealth(-5);
	TestTrue(TEXT("Actual player damage triggers feedback without a hit montage"), Stop->IsHitStopActive());
	Advance(10);
	ApplyPlayerHealth(5);
	TestFalse(TEXT("Healing does not trigger feedback"), Stop->IsHitStopActive());
	if (Feedback) Feedback->bEnableHitStop = false;
	ApplyPlayerHealth(-5);
	TestFalse(TEXT("Hit stop can be disabled independently"), Stop->IsHitStopActive());
	if (Feedback) Feedback->bEnableHitStop = true;
	Advance(10);
	ApplyPlayerHealth(-1000);
	TestTrue(TEXT("Lethal hit also triggers feedback"), Player->IsDead() && Stop->IsHitStopActive());
	Advance(12);
	TestEqual(TEXT("Death does not strand world in slow motion"), Settings->TimeDilation, 1.0f);
	Stop->RequestHitStop(0.1f, 0.03f);
	Stop->RestoreTime();
	TestEqual(TEXT("Explicit cleanup restores world time"), Settings->TimeDilation, 1.0f);
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
