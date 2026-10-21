#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeFeedbackTest, "TheLastMessage.Weapon.MeleeFeedbackLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMeleeFeedbackTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	UWeaponDataAsset* Swing = NewObject<UWeaponDataAsset>(Player);
	Swing->WeaponType = EWeaponType::LightMelee;
	Swing->MeleeFeedbackDelay = 0.1f;
	UWeaponDataAsset* HeavySwing = DuplicateObject<UWeaponDataAsset>(Swing, Player);
	HeavySwing->WeaponType = EWeaponType::HeavyMelee;
	const FObjectPropertyBase* Feedback = FindFProperty<FObjectPropertyBase>(APlayerBase::StaticClass(), TEXT("MeleeFeedbackData"));
	auto CurrentFeedback = [Feedback, Player]() { return Feedback->GetObjectPropertyValue_InContainer(Player); };
	Player->MulticastMeleeAttackFeedback(EWeaponSlot::Primary, Swing);
	TestEqual(TEXT("Delayed swing captures its weapon"), CurrentFeedback(), static_cast<UObject*>(Swing));
	Player->MulticastStopMeleeAttackFeedback(EWeaponSlot::Secondary);
	TestEqual(TEXT("Other slot cannot stop this swing"), CurrentFeedback(), static_cast<UObject*>(Swing));
	Player->MulticastStopMeleeAttackFeedback(EWeaponSlot::Primary);
	TestNull(TEXT("Cancel clears pending feedback"), CurrentFeedback());
	for (int32 Frame = 0; Frame < 20; ++Frame) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.01f); }
	TestNull(TEXT("Canceled delay does not restore feedback"), CurrentFeedback());
	Player->MulticastMeleeAttackFeedback(EWeaponSlot::Primary, Swing);
	Player->MulticastMeleeAttackFeedback(EWeaponSlot::Secondary, HeavySwing);
	Player->MulticastStopMeleeAttackFeedback(EWeaponSlot::Primary);
	TestEqual(TEXT("Previous slot cleanup preserves new swing"), CurrentFeedback(), static_cast<UObject*>(HeavySwing));
	UWeaponDataAsset* Ranged = NewObject<UWeaponDataAsset>(Player);
	Ranged->WeaponType = EWeaponType::LightRanged;
	Player->MulticastMeleeAttackFeedback(EWeaponSlot::Primary, Ranged);
	TestEqual(TEXT("Ranged data cannot replace melee presentation"), CurrentFeedback(), static_cast<UObject*>(HeavySwing));
	Player->MulticastStopMeleeAttackFeedback(EWeaponSlot::Secondary);
	Swing->MeleeFeedbackDelay = 0.0f;
	Swing->MeleeEffectSocket = TEXT("MissingSocket");
	Swing->MeleeTrailSocket = TEXT("MissingSocket");
	Player->MulticastMeleeAttackFeedback(EWeaponSlot::Primary, Swing);
	TestEqual(TEXT("Unassigned cosmetic assets are supported"), CurrentFeedback(), static_cast<UObject*>(Swing));
	Player->MulticastStopMeleeAttackFeedback(EWeaponSlot::Primary);
	TestNull(TEXT("Completion clears active presentation"), CurrentFeedback());
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
