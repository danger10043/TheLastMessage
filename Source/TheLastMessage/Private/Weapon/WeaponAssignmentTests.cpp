#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Weapon/WeaponBase.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "GAS/Effects/GE_WeaponEnergyCost.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponAssignmentTest, "TheLastMessage.Weapon.NumberMouseAssignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponAssignmentTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	TestEqual(TEXT("No selection without held action"), Player->GetHeldWeaponChoice(), INDEX_NONE);
	Player->SetWeaponChoiceHeld(2, true);
	TestEqual(TEXT("Started selects weapon 3"), Player->GetHeldWeaponChoice(), 2);
	Player->SetWeaponChoiceHeld(0, true);
	TestEqual(TEXT("Lowest held number wins"), Player->GetHeldWeaponChoice(), 0);
	Player->SetWeaponChoiceHeld(0, false);
	TestEqual(TEXT("Completed falls back to remaining key"), Player->GetHeldWeaponChoice(), 2);
	Player->SetWeaponChoiceHeld(2, false);
	TestEqual(TEXT("Completed or Canceled clears selection"), Player->GetHeldWeaponChoice(), INDEX_NONE);
	Player->WeaponChoices.SetNum(4);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UWeaponDataAsset* Data = NewObject<UWeaponDataAsset>(Player);
		Data->WeaponType = Index < 2 ? EWeaponType::LightMelee : EWeaponType::LightRanged;
		Data->MaxEnergy = Index < 2 ? 0.0f : 100.0f;
		Player->WeaponChoices[Index] = Data;
	}
	TestFalse(TEXT("Unowned weapon cannot be assigned"), Player->AssignWeaponChoice(0, EWeaponSlot::Primary));
	for (int32 Index = 0; Index < 4; ++Index) TestTrue(TEXT("Acquire weapon"), Player->AcquireWeaponChoice(Index));
	TestTrue(TEXT("1 + left assigns pipe"), Player->AssignWeaponChoice(0, EWeaponSlot::Primary));
	TestTrue(TEXT("3 + right assigns pistol"), Player->AssignWeaponChoice(2, EWeaponSlot::Secondary));
	UWeaponBase* Pipe = Player->GetPrimaryWeapon();
	UWeaponBase* Pistol = Player->GetSecondaryWeapon();
	Pistol->NextAttackAllowedTime = World->GetTimeSeconds() + 2.0;
	Pistol->LastSuccessfulFireTime = World->GetTimeSeconds();
	Player->GetWeaponSlotAttributes()->SetSharedEnergy(37.0f);
	TestTrue(TEXT("1 + right swaps assigned weapons"), Player->AssignWeaponChoice(0, EWeaponSlot::Secondary));
	TestTrue(TEXT("Pistol moves to left"), Player->GetPrimaryWeapon() == Pistol);
	TestTrue(TEXT("Pipe moves to right"), Player->GetSecondaryWeapon() == Pipe);
	TestEqual(TEXT("Shared energy unchanged after swap"), Player->GetWeaponSlotAttributes()->GetSharedEnergy(), 37.0f);
	TestTrue(TEXT("Pistol retains cooldown after swap"), Player->GetPrimaryWeapon()->NextAttackAllowedTime > World->GetTimeSeconds());
	TestTrue(TEXT("UI reports pipe on right"), Player->GetWeaponChoiceAssignment(0) == EWeaponSlot::Secondary);
	TestTrue(TEXT("UI reports pistol on left"), Player->GetWeaponChoiceAssignment(2) == EWeaponSlot::Primary);
	TestTrue(TEXT("Already assigned is a no-op"), Player->AssignWeaponChoice(2, EWeaponSlot::Primary));
	TestEqual(TEXT("No-op does not refill energy"), Player->GetWeaponSlotAttributes()->GetSharedEnergy(), 37.0f);
	TestTrue(TEXT("4 + left replaces pistol"), Player->AssignWeaponChoice(3, EWeaponSlot::Primary));
	TestTrue(TEXT("3 + left restores cached pistol"), Player->AssignWeaponChoice(2, EWeaponSlot::Primary));
	TestTrue(TEXT("Original weapon instance is reused"), Player->GetPrimaryWeapon() == Pistol);
	TestEqual(TEXT("Unequip and re-equip preserve shared energy"), Player->GetWeaponSlotAttributes()->GetSharedEnergy(), 37.0f);
	TestFalse(TEXT("Invalid index rejected"), Player->AssignWeaponChoice(4, EWeaponSlot::Primary));
	TestFalse(TEXT("Invalid slot rejected"), Player->AssignWeaponChoice(0, EWeaponSlot::None));
	Player->WeaponChoices[1] = nullptr;
	TestFalse(TEXT("Empty choice rejected"), Player->AssignWeaponChoice(1, EWeaponSlot::Primary));
	Player->GetAbilitySystemComponent()->AddLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	TestFalse(TEXT("Assignment cannot replace an active attack"), Player->AssignWeaponChoice(0, EWeaponSlot::Primary));
	Player->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WeaponGameplayTags::State_Attacking);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	for (TSubclassOf<UGameplayEffect> CostClass : { TSubclassOf<UGameplayEffect>(UGE_WeaponPrimaryEnergyCost::StaticClass()), TSubclassOf<UGameplayEffect>(UGE_WeaponSecondaryEnergyCost::StaticClass()) })
	{
		FGameplayEffectSpecHandle Cost = ASC->MakeOutgoingSpec(CostClass, 1, ASC->MakeEffectContext());
		Cost.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_EnergyCost, -10.0f);
		ASC->ApplyGameplayEffectSpecToSelf(*Cost.Data.Get());
	}
	TestEqual(TEXT("Both mouse attack costs use the shared pool"), Player->GetWeaponSlotAttributes()->GetSharedEnergy(), 17.0f);
	Player->GetWeaponSlotAttributes()->SetSharedMaxEnergy(10.0f);
	TestEqual(TEXT("Shared maximum reduction clamps energy"), Player->GetWeaponSlotAttributes()->GetSharedEnergy(), 10.0f);
	Player->HandleDeath();
	TestFalse(TEXT("Dead players cannot assign"), Player->AssignWeaponChoice(0, EWeaponSlot::Primary));
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
