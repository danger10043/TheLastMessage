#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Checkpoint/PlayerCheckpoint.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerCheckpointTest, "TheLastMessage.Checkpoint.RespawnLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerCheckpointTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	Controller->Possess(Player);
	Player->SetPlayerState(World->SpawnActor<APlayerState>());
	UCombatAttributeSet* Stats = Player->GetCombatAttributes();
	Stats->SetMaxHealth(100.0f);
	Stats->SetHealth(100.0f);
	const FVector Start = Player->GetActorLocation();
	Player->SetActorLocation(FVector(500, 0, 200));
	UGameplayEffect* Damage = NewObject<UGameplayEffect>();
	Damage->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCombatAttributeSet::GetIncomingDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1000.0f));
	Damage->Modifiers.Add(Modifier);
	Player->GetAbilitySystemComponent()->ApplyGameplayEffectToSelf(Damage, 1, Player->GetAbilitySystemComponent()->MakeEffectContext());
	TestTrue(TEXT("Lethal damage enters death state"), Player->IsDead());
	TestTrue(TEXT("Airborne death keeps gravity active"), Player->GetCharacterMovement()->IsFalling());
	const double DeathHeight = Player->GetActorLocation().Z;
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, 0.02f);
	}
	TestTrue(TEXT("Dead player falls instead of hovering"), Player->GetActorLocation().Z < DeathHeight);
	TestTrue(TEXT("No checkpoint uses initial spawn"), Player->RespawnAtCheckpoint());
	TestTrue(TEXT("Returned to initial spawn"), Player->GetActorLocation().Equals(Start, 1));
	TestEqual(TEXT("Health fully restored"), Stats->GetHealth(), 100.0f);
	TestFalse(TEXT("Death tag cleared"), Player->IsDead());
	TestTrue(TEXT("Movement enabled"), Player->GetCharacterMovement()->MovementMode == MOVE_Falling);

	const FVector Saved(1000, 0, 200);
	APlayerCheckpoint* Checkpoint = World->SpawnActor<APlayerCheckpoint>(Saved, FRotator::ZeroRotator);
	TestFalse(TEXT("Fast travel disabled by default"), Checkpoint->CanFastTravel());
    Checkpoint->bAllowFastTravel = true;
    TestFalse(TEXT("Unvisited enabled checkpoint cannot travel"), Checkpoint->CanFastTravel());
    TestFalse(TEXT("Unvisited destination rejects travel"), Checkpoint->FastTravel(Player));
    TestFalse(TEXT("Outline hidden before contact"), Checkpoint->InteractionOutline->IsVisible());
	Player->SetActorLocation(Saved);
	TestTrue(TEXT("Contact activates checkpoint"), Checkpoint->CheckpointVisualState == ECheckpointVisualState::Current);
	TestTrue(TEXT("Outline visible during contact"), Checkpoint->InteractionOutline->IsVisible());
	Player->SetActorLocation(FVector(1500, 0, 200));
	TestFalse(TEXT("Outline hidden after leaving"), Checkpoint->InteractionOutline->IsVisible());
    TestTrue(TEXT("Visited enabled checkpoint permits fast travel"), Checkpoint->CanFastTravel());
    Checkpoint->bAllowFastTravel = false;
    TestFalse(TEXT("Ordinary visited checkpoint rejects fast travel"), Checkpoint->FastTravel(Player));
    Checkpoint->bAllowFastTravel = true;
    TestTrue(TEXT("Visited destination teleports player"), Checkpoint->FastTravel(Player));
    TestTrue(TEXT("Fast travel reaches checkpoint"), Player->GetActorLocation().Equals(Saved,1));
    Player->SetActorLocation(FVector(1500,0,200));

	Player->HandleDeath();
    TestFalse(TEXT("Dead player cannot fast travel"), Checkpoint->FastTravel(Player));
	TestFalse(TEXT("Dead players cannot change checkpoint"), Player->SetCheckpoint(FTransform::Identity));
	TestTrue(TEXT("Checkpoint respawn succeeds"), Player->RespawnAtCheckpoint());
	TestTrue(TEXT("Contact saved checkpoint location"), Player->GetActorLocation().Equals(Saved, 1));
	TestFalse(TEXT("Cannot respawn twice while alive"), Player->RespawnAtCheckpoint());

	Checkpoint->SetActorLocation(FVector(2000, 0, 200));
	TestTrue(TEXT("Later checkpoint replaces previous"), Checkpoint->ActivateCheckpoint(Player));
	Player->SetActorLocation(FVector(2500, 0, 200));
	Player->FellOutOfWorld(*GetDefault<UDamageType>());
	for (int32 Frame = 0; Frame < 115; ++Frame)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, 0.02f);
	}
	TestFalse(TEXT("Automatic respawn after falling out"), Player->IsDead());
	TestFalse(TEXT("Pawn survives falling out"), Player->IsActorBeingDestroyed());
	TestTrue(TEXT("Automatic respawn uses latest checkpoint"), FMath::IsNearlyEqual(Player->GetActorLocation().X, 2000.0, 1.0));
	TestEqual(TEXT("Repeated respawn restores health"), Stats->GetHealth(), 100.0f);
	AActor* Obstacle = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Box);
	Obstacle->AddInstanceComponent(Box);
	Box->SetBoxExtent(FVector(500));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Obstacle->SetActorLocation(FVector(2000, 0, 200));
	Player->SetActorLocation(FVector(3500, 0, 200));
	Player->HandleDeath();
	TestTrue(TEXT("Occupied checkpoint respawns above obstruction"), Player->RespawnAtCheckpoint());
	TestFalse(TEXT("Raised respawn restores living state"), Player->IsDead());
	TestTrue(TEXT("Raised respawn clears obstruction"), Player->GetActorLocation().Z > 700.0f);
	Box->SetBoxExtent(FVector(5000));
	Player->SetActorLocation(FVector(8000, 0, 200));
	Player->HandleDeath();
	TestFalse(TEXT("Fully blocked search cannot revive inside geometry"), Player->RespawnAtCheckpoint());
	TestTrue(TEXT("Fully blocked respawn remains dead"), Player->IsDead());
	Obstacle->Destroy();
	TestTrue(TEXT("Respawn succeeds once obstruction clears"), Player->RespawnAtCheckpoint());
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
