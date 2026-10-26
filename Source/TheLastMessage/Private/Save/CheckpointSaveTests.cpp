#if WITH_DEV_AUTOMATION_TESTS
#include "Save/ProgressSaveSubsystem.h"
#include "Character/Player/PlayerBase.h"
#include "Checkpoint/PlayerCheckpoint.h"
#include "Core/CoreComponent.h"
#include "DataAsset/CoreDataAsset.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "UI/SaveNotificationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/StrongObjectPtr.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "Map/AreaMapComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheckpointSaveFlowTest, "TheLastMessage.Save.CheckpointSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCheckpointSaveFlowTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UProgressSaveSubsystem> Saves(NewObject<UProgressSaveSubsystem>(Instance.Get()));
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/CheckpointSave") / FGuid::NewGuid().ToString());
	Saves->AutomationSaveDirectory = Directory;
	TestTrue(TEXT("Slot two begins"), Saves->StartNewGame(2));
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	APlayerController* PC = World->SpawnActor<APlayerController>();
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player);
	Player->SetPlayerState(World->SpawnActor<APlayerState>());
	Player->WeaponChoices[0] = LoadObject<UWeaponDataAsset>(nullptr, TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Pipe.DA_Weapon_Pipe"));
	Player->AcquireWeaponChoice(0); Player->EquipWeapon(EWeaponSlot::Primary, Player->WeaponChoices[0]);
	UCoreDataAsset* Core = LoadObject<UCoreDataAsset>(nullptr, TEXT("/Game/Test_Jihun/Core/Ability/DataAsset/DA_Core_DoubleJump.DA_Core_DoubleJump"));
	if (Core) Player->GetCoreComponent()->AcquireCore(Core);
	TestTrue(TEXT("Core acquired for snapshot"), Player->GetCoreComponent()->GetOwnedCores().Num() > 0);
	Player->GetCombatAttributes()->SetMaxHealth(100); Player->GetCombatAttributes()->SetHealth(37);
	Player->GetWeaponSlotAttributes()->SetSharedEnergy(25);
	const FName MapName(*UGameplayStatics::GetCurrentLevelName(Player, true));
	Saves->GetWorkingProgress()->Maps.FindOrAdd(MapName).ExploredRooms.Add(TEXT("RoomA"));
	APlayerCheckpoint* First = World->SpawnActor<APlayerCheckpoint>(FVector(1000, 0, 200), FRotator::ZeroRotator);
	APlayerCheckpoint* Second = World->SpawnActor<APlayerCheckpoint>(FVector(2000, 0, 200), FRotator::ZeroRotator);
	TestTrue(TEXT("Stable checkpoint ID"), First->GetPersistentId() == First->GetPersistentId());
	TestTrue(TEXT("Distinct checkpoint IDs"), First->GetPersistentId() != Second->GetPersistentId());
	Player->SetActorLocation(First->GetActorLocation());
	TestTrue(TEXT("Contact still activates"), First->CheckpointVisualState == ECheckpointVisualState::Current);
	TestTrue(TEXT("First snapshot accepted"), Saves->SaveCheckpoint(Player, First));
	Player->GetCombatAttributes()->SetHealth(68);
	Player->SetActorLocation(Second->GetActorLocation());
	TestTrue(TEXT("Rapid contact queued"), Saves->SaveCheckpoint(Player, Second));
	const double Deadline = FPlatformTime::Seconds() + 10;
	while (Saves->IsSaving() && FPlatformTime::Seconds() < Deadline)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FPlatformProcess::Sleep(0.005f);
	}
	TestFalse(TEXT("Queued writes finish"), Saves->IsSaving());
	const FString File = Directory / TEXT("Progress_2.sav");
	TArray<uint8> Bytes;
	FFileHelper::LoadFileToArray(Bytes, *File);
	UProgressSaveGame* Loaded = Cast<UProgressSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("Checkpoint file created"), Loaded))
	{
		TestEqual(TEXT("Only current slot recorded"), Loaded->SlotId, 2);
		TestTrue(TEXT("Latest contact persisted"), Loaded->CheckpointId == Second->GetPersistentId());
		TestTrue(TEXT("Respawn point captured"), Loaded->RespawnTransform.GetLocation().Equals(Second->GetActorLocation()));
		TestEqual(TEXT("Health captured at contact"), Loaded->Player.Health, 68.f);
		TestEqual(TEXT("Energy captured"), Loaded->Player.Energy, 25.f);
		TestTrue(TEXT("Weapon ownership captured"), Loaded->Player.OwnedWeapons.Contains(FSoftObjectPath(Player->WeaponChoices[0].Get())));
		TestTrue(TEXT("Primary assignment captured"), Loaded->Player.PrimaryWeapon == FSoftObjectPath(Player->WeaponChoices[0].Get()));
		TestTrue(TEXT("Core level captured"), Loaded->Player.CoreLevels.Contains(FSoftObjectPath(Core)));
		TestTrue(TEXT("Exploration captured"), Loaded->Maps.FindChecked(MapName).ExploredRooms.Contains(TEXT("RoomA")));
		TestEqual(TEXT("Visited checkpoints retained"), Loaded->Maps.FindChecked(MapName).VisitedCheckpoints.Num(), 2);
		// Recreate the in-memory load result, then exercise the same application used after travel.
		Saves->WorkingProgress = Loaded;
		Saves->bRestorePending = true;
		Player->GetCoreComponent()->ResetOwnedCores();
		Player->SetActorLocation(FVector::ZeroVector);
		Player->SetCheckpoint(FTransform::Identity);
		Player->OwnedWeaponChoices.Init(false, 4);
		Player->GetCombatAttributes()->SetHealth(25);
		First->SetCheckpointVisualState(ECheckpointVisualState::Inactive);
		Second->SetCheckpointVisualState(ECheckpointVisualState::Inactive);
		TestTrue(TEXT("Loaded checkpoint applies to player"), Saves->RestorePlayerIfPending(Player));
		TestTrue(TEXT("Spawn restored to saved checkpoint"), Player->GetActorLocation().Equals(Loaded->RespawnTransform.GetLocation(), 1));
		TestTrue(TEXT("Owned core restored"), Player->GetCoreComponent()->HasCore(Core->CoreId));
		TestEqual(TEXT("Saved core level restored"), Player->GetCoreComponent()->GetCoreLevel(Core->CoreId), Loaded->Player.CoreLevels.FindChecked(FSoftObjectPath(Core)));
		TestEqual(TEXT("Saved health restored"), Player->GetCombatAttributes()->GetHealth(), 68.f);
		TestTrue(TEXT("Weapon ownership restored"), Player->HasWeaponChoice(0));
		TestTrue(TEXT("Primary weapon restored"), Player->GetWeaponDataInSlot(EWeaponSlot::Primary) == Player->WeaponChoices[0]);
		TestTrue(TEXT("Map exploration restored"), Player->FindComponentByClass<UAreaMapComponent>()->IsExplored(TEXT("RoomA")));
		TestTrue(TEXT("Latest checkpoint display restored"), Second->CheckpointVisualState == ECheckpointVisualState::Current);
		TestTrue(TEXT("Earlier checkpoint display restored"), First->CheckpointVisualState == ECheckpointVisualState::Visited);
		TestFalse(TEXT("Load teleport does not write a save"), Saves->IsSaving());
		TestFalse(TEXT("Restoration runs once"), Saves->RestorePlayerIfPending(Player));
		Player->InitializePlayer();
		TestEqual(TEXT("Initialization does not duplicate cores"), Player->GetCoreComponent()->GetOwnedCores().Num(), Loaded->Player.CoreLevels.Num());
		Player->SetActorLocation(FVector(3000, 0, 200)); Player->HandleDeath();
		TestTrue(TEXT("Loaded checkpoint becomes respawn location"), Player->RespawnAtCheckpoint());
		TestTrue(TEXT("Death respawn uses loaded checkpoint"), Player->GetActorLocation().Equals(Loaded->RespawnTransform.GetLocation(), 1));
	}
	TestFalse(TEXT("No slot one created"), Saves->HasSave(1)); TestFalse(TEXT("No slot three created"), Saves->HasSave(3));
	USaveNotificationWidget* Popup = CreateWidget<USaveNotificationWidget>(PC, USaveNotificationWidget::StaticClass());
	Popup->TakeWidget(); Popup->DisplayDuration = 0.1f;
	TestTrue(TEXT("Popup hidden initially"), Popup->GetVisibility() == ESlateVisibility::Collapsed);
	Popup->ShowResult(true);
	TestTrue(TEXT("Success popup shown"), Popup->GetVisibility() == ESlateVisibility::HitTestInvisible);
	for (int32 Frame = 0; Frame < 3; ++Frame) { ++GFrameCounter; World->GetTimerManager().Tick(0.1f); }
	TestTrue(TEXT("Popup expires"), Popup->GetVisibility() == ESlateVisibility::Collapsed);
	Popup->ShowResult(false);
	TestTrue(TEXT("Failure is not success text"), Popup->WidgetTree->FindWidget<UTextBlock>(TEXT("MessageText"))->GetText().ToString().Contains(TEXT("못했습니다")));
	Popup->RemoveFromParent();
	World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	Saves->EndSession();
	IFileManager::Get().Delete(*File); IFileManager::Get().DeleteDirectory(*Directory, false, false);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheckpointRestoreStartupTest, "TheLastMessage.Save.RestoreAfterPlayerStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCheckpointRestoreStartupTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	UProgressSaveSubsystem* Saves = Instance->GetSubsystem<UProgressSaveSubsystem>();
	Saves->StartNewGame(2);
	UProgressSaveGame* Progress = Saves->GetWorkingProgress();
	Progress->MapPackage = World->GetOutermost()->GetName();
	Progress->CheckpointId = FGuid::NewGuid();
	Progress->RespawnTransform = FTransform(FVector(3000, 0, 200));
	Progress->Player.Health = 68;
	UCoreDataAsset* Core = LoadObject<UCoreDataAsset>(nullptr, TEXT("/Game/Test_Jihun/Core/Ability/DataAsset/DA_Core_DoubleJump.DA_Core_DoubleJump"));
	Progress->Player.CoreLevels.Add(FSoftObjectPath(Core), 1);
	Saves->bRestorePending = true;
	World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
	APlayerBase* Player = World->SpawnActor<APlayerBase>();
	Player->GetCombatAttributes()->SetMaxHealth(100); Player->GetCombatAttributes()->SetHealth(100);
	APlayerController* PC = World->SpawnActor<APlayerController>();
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player);
	// Run the deferred timer registered by BeginPlay/PossessedBy, without calling restore directly.
	Player->SetPlayerState(World->SpawnActor<APlayerState>());
	TestTrue(TEXT("Startup world retains its game instance"), Player->GetGameInstance() == Instance.Get());
	TestTrue(TEXT("Startup actor began play"), Player->HasActorBegunPlay());
	for (int32 Frame = 0; Frame < 3; ++Frame) { ++GFrameCounter; World->GetTimerManager().Tick(0.02f); }
	TestFalse(TEXT("Startup consumes pending restoration"), Saves->IsRestoringProgress());
	TestTrue(TEXT("Startup spawns at loaded checkpoint"), Player->GetActorLocation().Equals(Progress->RespawnTransform.GetLocation(), 1));
	TestTrue(TEXT("Startup restores saved core"), Player->GetCoreComponent()->HasCore(Core->CoreId));
	TestEqual(TEXT("Deferred callbacks do not duplicate core grants"), Player->GetCoreComponent()->GetOwnedCores().Num(), 1);
	TestEqual(TEXT("Startup restores health"), Player->GetCombatAttributes()->GetHealth(), 68.f);
	TestFalse(TEXT("Loading does not autosave"), Saves->IsSaving());
	World->EndPlay(EEndPlayReason::Quit);
	Instance->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return !HasAnyErrors();
}
#endif
