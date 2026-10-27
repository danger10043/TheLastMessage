#if WITH_DEV_AUTOMATION_TESTS
#include "Cinematics/OnceCinematicController.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossPatternComponent.h"
#include "UObject/UnrealType.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCinematicControllerTest, "TheLastMessage.Cinematics.OnceAndLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCinematicControllerTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	UProgressSaveSubsystem* Saves = Instance->GetSubsystem<UProgressSaveSubsystem>();
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/CinematicController") / FGuid::NewGuid().ToString());
	Saves->AutomationSaveDirectory = Directory;
	Saves->StartNewGame(1);
	Saves->GetWorkingProgress()->MapPackage = TEXT("/Game/Test_Jihong/Prototype/Lv_Factory_Metroidvania");
	World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	APlayerController* PC = World->SpawnActor<APlayerController>();
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player);
	const bool bInput = Player->InputEnabled();
	ULevelSequence* Sequence = NewObject<ULevelSequence>(); Sequence->Initialize();
	Sequence->GetMovieScene()->SetPlaybackRange(0, 24000);
	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* Playback = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, FMovieSceneSequencePlaybackSettings(), SequenceActor);
	AOnceCinematicController* Scene = World->SpawnActor<AOnceCinematicController>();
	Scene->CinematicId = TEXT("TestScene"); Scene->SequenceActor = SequenceActor;
	TestTrue(TEXT("First playback starts"), Scene->RequestPlay());
	TestTrue(TEXT("Playback hides the player and disables input"), Player->IsHidden() && !Player->InputEnabled());
	Playback->Stop();
	TestTrue(TEXT("Stopping restores visibility and input"), !Player->IsHidden() && Player->InputEnabled() == bInput);
	TestFalse(TEXT("Consumed controller cannot replay"), Scene->RequestPlay());
	AOnceCinematicController* Duplicate = World->SpawnActor<AOnceCinematicController>();
	Duplicate->CinematicId = TEXT("TestScene"); Duplicate->SequenceActor = SequenceActor;
	TestFalse(TEXT("A duplicate controller cannot replay the same saved ID"), Duplicate->RequestPlay());
	// Detection near a checkpoint must not expose a boss bar before the intro trigger.
	ABossMonster* Boss = World->SpawnActorDeferred<ABossMonster>(ABossMonster::StaticClass(), FTransform::Identity);
	Boss->AutoPossessAI = EAutoPossessAI::Disabled;
	FindFProperty<FClassProperty>(ABossMonster::StaticClass(), TEXT("InitialStatsEffect"))
		->SetObjectPropertyValue_InContainer(Boss, UGameplayEffect::StaticClass());
	Boss->FinishSpawning(FTransform::Identity);
	auto BossUIAllowed = [Boss]()
	{
		return FindFProperty<FBoolProperty>(ABossMonster::StaticClass(), TEXT("bBossUIVisible"))->GetPropertyValue_InContainer(Boss);
	};
	AOnceCinematicController* BossScene = World->SpawnActorDeferred<AOnceCinematicController>(
		AOnceCinematicController::StaticClass(), FTransform::Identity);
	BossScene->Boss = Boss; BossScene->CinematicId = TEXT("BossIntro"); BossScene->SequenceActor = SequenceActor;
	BossScene->FinishSpawning(FTransform::Identity);
	Boss->SetCurrentTarget(Player);
	TestFalse(TEXT("Detecting the player before the intro does not enable the boss UI"), BossUIAllowed());
	Boss->SetBossState(EBossState::Attacking);
	UBossPatternComponent* Pattern = Boss->FindComponentByClass<UBossPatternComponent>();
	FBoolProperty* Executing = FindFProperty<FBoolProperty>(UBossPatternComponent::StaticClass(), TEXT("bIsAttackExecuting"));
	Executing->SetPropertyValue_InContainer(Pattern, true);
	TestTrue(TEXT("First boss intro starts"), BossScene->RequestPlay());
	TestTrue(TEXT("Boss enters the cinematic state that rejects attack abilities"), Boss->GetBossState() == EBossState::Cinematic);
	TestFalse(TEXT("The in-progress attack is cancelled"), Executing->GetPropertyValue_InContainer(Pattern));
	TestFalse(TEXT("Boss UI stays suppressed during the intro"), BossUIAllowed());
	Playback->Stop();
	TestTrue(TEXT("Stopping the intro returns a cancelled attack to combat"), Boss->GetBossState() == EBossState::Combat);
	TestTrue(TEXT("Boss UI is allowed after the intro ends"), BossUIAllowed());
	AOnceCinematicController* LoadedBossScene = World->SpawnActorDeferred<AOnceCinematicController>(
		AOnceCinematicController::StaticClass(), FTransform::Identity);
	LoadedBossScene->Boss = Boss; LoadedBossScene->CinematicId = TEXT("BossIntro");
	LoadedBossScene->FinishSpawning(FTransform::Identity);
	Boss->SetCurrentTarget(Player);
	TestFalse(TEXT("A seen intro still gates the boss UI until its trigger is entered"), BossUIAllowed());
	TestFalse(TEXT("Entering a seen boss intro skips playback"), LoadedBossScene->RequestPlay());
	TestTrue(TEXT("Skipping a seen boss intro enables the boss UI for combat"), BossUIAllowed());
	TestTrue(TEXT("Skipping a seen intro leaves combat running"), Boss->GetBossState() == EBossState::Combat);
	AOnceCinematicController* InterruptedBossScene = World->SpawnActor<AOnceCinematicController>();
	InterruptedBossScene->Boss = Boss; InterruptedBossScene->CinematicId = TEXT("InterruptedBossIntro");
	InterruptedBossScene->SequenceActor = SequenceActor;
	TestTrue(TEXT("Another boss intro can start"), InterruptedBossScene->RequestPlay());
	InterruptedBossScene->Destroy();
	TestTrue(TEXT("Removing the controller restores boss combat"), Boss->GetBossState() == EBossState::Combat);
	Saves->bLoadedSession = true;
	Saves->LoadedMap = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	AActor* StandIn = World->SpawnActor<AActor>();
	AOnceCinematicController* Entry = World->SpawnActor<AOnceCinematicController>();
	Entry->CinematicId = TEXT("Entry"); Entry->bSkipOnLoad = true; Entry->CleanupActors.Add(StandIn);
	TestFalse(TEXT("Loaded entry skips even with no sequence available"), Entry->RequestPlay());
	TestTrue(TEXT("Skipped entry removes the cinematic stand-in"), StandIn->IsActorBeingDestroyed());
	TestTrue(TEXT("Skipped entry does not hide or disable the player"), !Player->IsHidden() && Player->InputEnabled() == bInput);
	const double Deadline = FPlatformTime::Seconds() + 10;
	while (Saves->IsSaving() && FPlatformTime::Seconds() < Deadline)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FPlatformProcess::Sleep(0.005f);
	}
	TestFalse(TEXT("Cinematic saves finish"), Saves->IsSaving());
	World->EndPlay(EEndPlayReason::Quit); Instance->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	IFileManager::Get().Delete(*(Directory / TEXT("Progress_1.sav")));
	IFileManager::Get().DeleteDirectory(*Directory, false, false);
	return !HasAnyErrors();
}
#endif
