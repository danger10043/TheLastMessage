#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Gimmick/Component/GravityStateComponent.h"
#include "Gimmick/Plate/GravityFlipPlate.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerGravityTest, "TheLastMessage.Player.NativeGravityComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerGravityTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	UClass* PlayerBP = LoadClass<APlayerBase>(nullptr, TEXT("/Game/Test_Jihong/BP_TestPlayer.BP_TestPlayer_C"));
	TestNotNull(TEXT("Actual player BP loads"), PlayerBP);
	const FGameplayTag Reversed = FGameplayTag::RequestGameplayTag(TEXT("State.Gravity.Reversed"));
	int32 Index = 0;
	for (UClass* PlayerClass : {APlayerBase::StaticClass(), PlayerBP})
	{
		if (!PlayerClass) continue;
		APlayerBase* Player = World->SpawnActor<APlayerBase>(PlayerClass, FVector(Index++ * 2000, 0, 300), FRotator::ZeroRotator);
		Player->GetCombatAttributes()->SetMaxHealth(100);
		Player->GetCombatAttributes()->SetHealth(100);
		TArray<UGravityStateComponent*> Components;
		Player->GetComponents<UGravityStateComponent>(Components);
		TestEqual(TEXT("Exactly one gravity component per player"), Components.Num(), 1);
		if (Components.IsEmpty()) continue;
		TestTrue(TEXT("Gravity component is created by native constructor"), Components[0]->CreationMethod == EComponentCreationMethod::Native);
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		TestTrue(TEXT("Initial gravity points down"), Movement->GetGravityDirection().Equals(FVector(0, 0, -1)));
		AGravityFlipPlate* Plate = World->SpawnActor<AGravityFlipPlate>(FVector(10000, 0, 300), FRotator::ZeroRotator);
		Plate->HandlePlateEnter(Player);
		TestTrue(TEXT("Existing plate applies configured gravity GE"), ASC->HasMatchingGameplayTag(Reversed));
		TestTrue(TEXT("Gravity reverses up"), Movement->GetGravityDirection().Equals(FVector(0, 0, 1)));
		TestTrue(TEXT("Capsule aligns to reversed gravity"), Player->GetActorUpVector().Equals(FVector(0, 0, -1), 0.001f));
		Plate->HandlePlateEnter(Player);
		TestTrue(TEXT("Duplicate overlap does not toggle again"), ASC->HasMatchingGameplayTag(Reversed));
		Plate->HandlePlateExit(Player);
		Plate->HandlePlateEnter(Player);
		TestFalse(TEXT("Re-entering restores normal gravity"), ASC->HasMatchingGameplayTag(Reversed));
		TestTrue(TEXT("Normal capsule orientation restored"), Player->GetActorUpVector().Equals(FVector::UpVector, 0.001f));
		Plate->HandlePlateExit(Player);
		Player->SetCheckpoint(Player->GetActorTransform());
		Components[0]->ToggleGravityState();
		Player->HandleDeath();
		Components[0]->ToggleGravityState();
		TestTrue(TEXT("Dead player cannot toggle gravity"), ASC->HasMatchingGameplayTag(Reversed));
		TestTrue(TEXT("Respawn succeeds after gravity flip"), Player->RespawnAtCheckpoint());
		TestFalse(TEXT("Respawn restores checkpoint gravity tag"), ASC->HasMatchingGameplayTag(Reversed));
		TestTrue(TEXT("Respawn restores downward gravity"), Movement->GetGravityDirection().Equals(FVector(0, 0, -1)));
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
