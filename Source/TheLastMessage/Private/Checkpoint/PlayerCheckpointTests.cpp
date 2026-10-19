#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/Player/PlayerBase.h"
#include "Checkpoint/PlayerCheckpoint.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
	TestTrue(TEXT("No checkpoint uses initial spawn"), Player->RespawnAtCheckpoint());
	TestTrue(TEXT("Returned to initial spawn"), Player->GetActorLocation().Equals(Start, 1));
	TestEqual(TEXT("Health fully restored"), Stats->GetHealth(), 100.0f);
	TestFalse(TEXT("Death tag cleared"), Player->IsDead());
	TestTrue(TEXT("Movement enabled"), Player->GetCharacterMovement()->MovementMode == MOVE_Falling);

	const FVector Saved(1000, 0, 200);
	APlayerCheckpoint* Checkpoint = World->SpawnActor<APlayerCheckpoint>(Saved, FRotator::ZeroRotator);
	// Exercise the real capsule overlap path, not only the callable activation API.
	Player->SetActorLocation(Saved);
	Player->SetActorLocation(FVector(1500, 0, 200));
	Player->HandleDeath();
	TestFalse(TEXT("Dead players cannot change checkpoint"), Player->SetCheckpoint(FTransform::Identity));
	TestTrue(TEXT("Checkpoint respawn succeeds"), Player->RespawnAtCheckpoint());
	TestTrue(TEXT("Overlap saved checkpoint location"), Player->GetActorLocation().Equals(Saved, 1));
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
	TestFalse(TEXT("Blocked checkpoint cannot revive inside geometry"), Player->RespawnAtCheckpoint());
	TestTrue(TEXT("Blocked respawn remains dead"), Player->IsDead());
	Obstacle->Destroy();
	TestTrue(TEXT("Respawn succeeds once obstruction clears"), Player->RespawnAtCheckpoint());
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
