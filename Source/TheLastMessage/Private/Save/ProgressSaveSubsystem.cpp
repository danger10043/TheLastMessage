#include "Core/CorePickup.h"
#include "Character/Boss/BossMonster.h"
#include "Projectile/ProjectileBase.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Character/Player/PlayerBase.h"
#include "Checkpoint/PlayerCheckpoint.h"
#include "Core/CoreComponent.h"
#include "Core/CorePickupStateSubsystem.h"
#include "Engine/GameInstance.h"
#include "DataAsset/CoreDataAsset.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Map/AreaMapComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Gimmick/GimmickBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

FString UProgressSaveSubsystem::GetSaveDirectory() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (!AutomationSaveDirectory.IsEmpty()) return AutomationSaveDirectory;
#endif
#if WITH_EDITOR
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames/Development"));
#else
	// Standard Windows package: <distribution>/<project>/Binaries/Win64/<game>.exe.
	return FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir(), TEXT("../../../SaveGames"));
#endif
}

FString UProgressSaveSubsystem::GetSlotPath(int32 SlotId) const
{
	return GetSaveDirectory() / FString::Printf(TEXT("Progress_%d.sav"), SlotId);
}

bool UProgressSaveSubsystem::HasSave(int32 SlotId) const
{
	return SlotId >= 1 && SlotId <= 3 && IFileManager::Get().FileExists(*GetSlotPath(SlotId));
}

UProgressSaveGame* UProgressSaveSubsystem::ReadSlot(int32 SlotId) const
{
	if (SlotId < 1 || SlotId > 3) return nullptr;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *GetSlotPath(SlotId))) return nullptr;
	UProgressSaveGame* Loaded = Cast<UProgressSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!Loaded || Loaded->Version != UProgressSaveGame::CurrentVersion || Loaded->SlotId != SlotId
		|| !Loaded->PlaythroughId.IsValid() || !FPackageName::IsValidLongPackageName(Loaded->MapPackage)
		|| !FPackageName::DoesPackageExist(Loaded->MapPackage)) return nullptr;
	return Loaded;
}

FProgressSlotInfo UProgressSaveSubsystem::GetSlotInfo(int32 SlotId) const
{
	FProgressSlotInfo Info;
	Info.bExists = HasSave(SlotId);
	if (const UProgressSaveGame* Save = ReadSlot(SlotId))
	{
		Info.bCanLoad = true;
		Info.bHasCheckpoint = Save->CheckpointId.IsValid();
		Info.SavedAt = Save->SavedAt;
		Info.MapPackage = Save->MapPackage;
	}
	return Info;
}

bool UProgressSaveSubsystem::StartNewGame(int32 SlotId, bool bOverwriteExisting)
{
	if (ActiveSlot || bSaving || SlotId < 1 || SlotId > 3 || (HasSave(SlotId) && !bOverwriteExisting)) return false;
	PendingGimmickRestores.Reset();
	WorkingProgress = NewObject<UProgressSaveGame>(this);
	if (UCorePickupStateSubsystem* Pickups = GetGameInstance()->GetSubsystem<UCorePickupStateSubsystem>())
		Pickups->ResetPickupStates();
	bLoadedSession = false;
	LoadedMap = NAME_None;
	WorkingProgress->SlotId = SlotId;
	WorkingProgress->PlaythroughId = FGuid::NewGuid();
	ActiveSlot = SlotId;
	return true;
}

bool UProgressSaveSubsystem::ContinueGame(int32 SlotId)
{
	if (ActiveSlot || bSaving) return false;
	UProgressSaveGame* Loaded = ReadSlot(SlotId);
	if (!Loaded) return false;
	PendingGimmickRestores.Reset();
	WorkingProgress = Loaded;
	// Restore before travelling: pickups query this registry before their Blueprint BeginPlay.
	if (UCorePickupStateSubsystem* Pickups = GetGameInstance()->GetSubsystem<UCorePickupStateSubsystem>())
		Pickups->RestorePickupStates(Loaded->CorePickupStates);
	bLoadedSession = true;
	LoadedMap = FName(*FPackageName::GetShortName(Loaded->MapPackage));
	bRestorePending = Loaded->CheckpointId.IsValid();
	ActiveSlot = SlotId;
	return true;
}

bool UProgressSaveSubsystem::EndSession()
{
	if (IsSaving()) return false;
	PendingCheckpointSnapshot = nullptr;
	bRestorePending = false;
	PendingGimmickRestores.Reset();
	WorkingProgress = nullptr;
	if (UCorePickupStateSubsystem* Pickups = GetGameInstance()->GetSubsystem<UCorePickupStateSubsystem>())
		Pickups->ResetPickupStates();
	bLoadedSession = false;
	LoadedMap = NAME_None;
	ActiveSlot = 0;
	return true;
}

bool UProgressSaveSubsystem::ReloadActiveProgress()
{
	if (IsSaving() || ActiveSlot < 1 || ActiveSlot > 3) return false;
	UProgressSaveGame* Loaded = ReadSlot(ActiveSlot);
	if (!Loaded || !Loaded->CheckpointId.IsValid()) return false;
	PendingGimmickRestores.Reset();
	WorkingProgress = Loaded;
	// Replace unsaved pickups too, so death/reload rolls back to the checkpoint snapshot.
	if (UCorePickupStateSubsystem* Pickups = GetGameInstance()->GetSubsystem<UCorePickupStateSubsystem>())
		Pickups->RestorePickupStates(Loaded->CorePickupStates);
	bLoadedSession = true;
	LoadedMap = FName(*FPackageName::GetShortName(Loaded->MapPackage));
	bRestorePending = true;
	return true;
}

bool UProgressSaveSubsystem::HasPlayedCinematic(FName Map, FName Cinematic) const
{
	const FProgressMapState* State = WorkingProgress ? WorkingProgress->Maps.Find(Map) : nullptr;
	return State && State->PlayedCinematics.Contains(Cinematic);
}

bool UProgressSaveSubsystem::MarkCinematicPlayed(FName Map, FName Cinematic)
{
	if (!WorkingProgress || ActiveSlot <= 0 || Map.IsNone() || Cinematic.IsNone() || HasPlayedCinematic(Map, Cinematic)) return false;
	WorkingProgress->Maps.FindOrAdd(Map).PlayedCinematics.Add(Cinematic);
	// A queued checkpoint must not overwrite newer cinematic history.
	if (PendingCheckpointSnapshot) PendingCheckpointSnapshot->Maps.FindOrAdd(Map).PlayedCinematics.Add(Cinematic);
	if (bSaving) bCinematicHistoryDirty = true;
	else SaveActiveProgress(); // Persist history without advancing the checkpoint/player snapshot.
	return true;
}

bool UProgressSaveSubsystem::SaveCheckpoint(APlayerBase* Player, APlayerCheckpoint* Checkpoint)
{
	if (bRestoringGimmicks) return false;
	if (!PendingGimmickRestores.IsEmpty()) RestoreQueuedGimmicks();
	if (IsRestoringProgress()) return false;
	if (!WorkingProgress || !IsValid(Player) || !IsValid(Checkpoint) || !Player->HasAuthority()
		|| Player->IsDead() || Player->GetWorld() != Checkpoint->GetWorld())
	{
		OnCheckpointSaveFinished.Broadcast(false);
		return false;
	}
	if (UAreaMapComponent* Map = Player->FindComponentByClass<UAreaMapComponent>())
	{
		Map->ExploreAt(Player->GetActorLocation());
		Map->SaveExploration();
	}
	UProgressSaveGame* Snapshot = DuplicateObject<UProgressSaveGame>(WorkingProgress, this);
	if (const UCorePickupStateSubsystem* Pickups = GetGameInstance()->GetSubsystem<UCorePickupStateSubsystem>())
		Snapshot->CorePickupStates = Pickups->GetPickupStates();
	Snapshot->MapPackage = UWorld::RemovePIEPrefix(Player->GetWorld()->GetOutermost()->GetName());
	Snapshot->CheckpointId = Checkpoint->GetPersistentId();
	Snapshot->RespawnTransform = Checkpoint->RespawnPoint->GetComponentTransform();
	Snapshot->RespawnGravity = Player->GetCharacterMovement()->GetGravityDirection();
	FProgressPlayerState& State = Snapshot->Player;
	State.OwnedWeapons.Reset(); State.CoreLevels.Reset();
	for (int32 Index = 0; Index < Player->WeaponChoices.Num(); ++Index)
		if (Player->HasWeaponChoice(Index) && Player->GetWeaponChoice(Index))
			State.OwnedWeapons.AddUnique(FSoftObjectPath(Player->GetWeaponChoice(Index)));
	State.PrimaryWeapon = FSoftObjectPath(Player->GetWeaponDataInSlot(EWeaponSlot::Primary));
	State.SecondaryWeapon = FSoftObjectPath(Player->GetWeaponDataInSlot(EWeaponSlot::Secondary));
	if (UCoreComponent* Cores = Player->GetCoreComponent())
		for (const FOwnedCoreEntry& Entry : Cores->GetOwnedCores())
			if (Entry.CoreData) State.CoreLevels.Add(FSoftObjectPath(Entry.CoreData.Get()), Entry.Level);
	if (const UCombatAttributeSet* Stats = Player->GetCombatAttributes())
	{
		State.Health = Stats->GetHealth(); State.MaxHealth = Stats->GetMaxHealth();
	}
	if (const UWeaponSlotAttributeSet* Stats = Player->GetWeaponSlotAttributes())
	{
		State.Energy = Stats->GetSharedEnergy(); State.MaxEnergy = Stats->GetSharedMaxEnergy();
	}
	FProgressMapState& Map = Snapshot->Maps.FindOrAdd(FName(*UGameplayStatics::GetCurrentLevelName(Player, true)));
	// Keep records of unloaded streamed actors; update the currently loaded placed actors.
	TSet<FGuid> CapturedIds;
	for (TActorIterator<AGimmickBase> It(Player->GetWorld()); It; ++It)
	{
		if (!It->ShouldPersist()) continue;
		const FGuid Id = It->GetPersistentId();
		if (CapturedIds.Contains(Id))
		{
			UE_LOG(LogTemp, Error, TEXT("Duplicate gimmick save ID: %s"), *It->GetPathName());
			OnCheckpointSaveFinished.Broadcast(false);
			return false;
		}
		CapturedIds.Add(Id);
		Map.Actors.Add(Id, It->CaptureProgress());
	}
	Map.VisitedCheckpoints.Add(Snapshot->CheckpointId);
	for (TActorIterator<APlayerCheckpoint> It(Player->GetWorld()); It; ++It)
		if (It->CheckpointVisualState != ECheckpointVisualState::Inactive)
			Map.VisitedCheckpoints.Add(It->GetPersistentId());
	if (bSaving) PendingCheckpointSnapshot = Snapshot; // Coalesce rapid contacts to the latest snapshot.
	else WriteCheckpointSnapshot(Snapshot);
	return true;
}

bool UProgressSaveSubsystem::RestorePlayerIfPending(APlayerBase* Player)
{
	if (bRestorePending && IsValid(Player))
		UE_LOG(LogTemp, Verbose, TEXT("Checkpoint restore: authority=%d controlled=%d begun=%d map=%s saved=%s"),
			Player->HasAuthority(), Player->IsPlayerControlled(), Player->HasActorBegunPlay(),
			*UWorld::RemovePIEPrefix(Player->GetWorld()->GetOutermost()->GetName()), WorkingProgress ? *WorkingProgress->MapPackage : TEXT("none"));
	if (!bRestorePending || bRestoring || !WorkingProgress || !IsValid(Player) || !Player->HasAuthority()
		|| !Player->IsPlayerControlled() || !Player->HasActorBegunPlay()) return false;
	if (UWorld::RemovePIEPrefix(Player->GetWorld()->GetOutermost()->GetName()) != WorkingProgress->MapPackage) return false;
	TGuardValue<bool> Guard(bRestoring, true);
	if (!Player->RestoreCheckpointProgress(WorkingProgress))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to restore checkpoint progress for slot %d"), ActiveSlot);
		return false;
	}
	bRestorePending = false;
	for (TActorIterator<APlayerCheckpoint> It(Player->GetWorld()); It; ++It) ApplyCheckpointState(*It);
	return true;
}

void UProgressSaveSubsystem::ApplyCheckpointState(APlayerCheckpoint* Checkpoint) const
{
	if (!WorkingProgress || !IsValid(Checkpoint) || !Checkpoint->HasAuthority()
		|| UWorld::RemovePIEPrefix(Checkpoint->GetWorld()->GetOutermost()->GetName()) != WorkingProgress->MapPackage) return;
	const FProgressMapState* Map = WorkingProgress->Maps.Find(FName(*UGameplayStatics::GetCurrentLevelName(Checkpoint, true)));
	const FGuid Id = Checkpoint->GetPersistentId();
	Checkpoint->SetCheckpointVisualState(Id == WorkingProgress->CheckpointId ? ECheckpointVisualState::Current
		: Map && Map->VisitedCheckpoints.Contains(Id) ? ECheckpointVisualState::Visited : ECheckpointVisualState::Inactive);
}

void UProgressSaveSubsystem::WriteCheckpointSnapshot(UProgressSaveGame* Snapshot)
{
	WorkingProgress = Snapshot;
	bCheckpointWrite = true;
	if (!SaveActiveProgress())
	{
		bCheckpointWrite = false;
		OnCheckpointSaveFinished.Broadcast(false);
	}
}

bool UProgressSaveSubsystem::SaveActiveProgress()
{
	if (bSaving || !WorkingProgress || ActiveSlot <= 0 || WorkingProgress->SlotId != ActiveSlot) return false;
	WorkingProgress->SavedAt = FDateTime::UtcNow();
	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(WorkingProgress, Bytes)) return false;
	bCinematicHistoryDirty = false;
	const FString Path = GetSlotPath(ActiveSlot);
	bSaving = true;
	TWeakObjectPtr<UProgressSaveSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, Path, Bytes = MoveTemp(Bytes)]()
	{
		IFileManager& Files = IFileManager::Get();
		// Staging file is never exposed as another loadable save slot.
		const FString Pending = Path + TEXT(".pending");
		const bool bSuccess = Files.MakeDirectory(*FPaths::GetPath(Path), true)
			&& FFileHelper::SaveArrayToFile(Bytes, *Pending)
			&& Files.Move(*Path, *Pending, true, true);
		if (!bSuccess) Files.Delete(*Pending);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess]()
		{
			if (UProgressSaveSubsystem* Self = WeakThis.Get())
			{
				Self->bSaving = false;
				const bool bWasCheckpoint = Self->bCheckpointWrite;
				Self->bCheckpointWrite = false;
				Self->OnSaveFinished.Broadcast(bSuccess);
				if (bWasCheckpoint) Self->OnCheckpointSaveFinished.Broadcast(bSuccess);
				if (Self->PendingCheckpointSnapshot)
				{
					UProgressSaveGame* Pending = Self->PendingCheckpointSnapshot;
					Self->PendingCheckpointSnapshot = nullptr;
					Self->WriteCheckpointSnapshot(Pending);
				}
				else if (Self->bCinematicHistoryDirty)
				{
					Self->bCinematicHistoryDirty = false;
					if (!Self->SaveActiveProgress()) Self->OnSaveFinished.Broadcast(false);
				}
			}
		});
	});
	return true;
}

void UProgressSaveSubsystem::QueueGimmickRestore(AGimmickBase* Gimmick)
{
    if (!IsValid(Gimmick) || !Gimmick->ShouldPersist() || !WorkingProgress) return;
    PendingGimmickRestores.AddUnique(TWeakObjectPtr<AGimmickBase>(Gimmick));
    // Every actor in this load wave finishes its derived BeginPlay before either restore pass.
    Gimmick->GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UProgressSaveSubsystem::RestoreQueuedGimmicks);
}

void UProgressSaveSubsystem::RestoreQueuedGimmicks()
{
    TArray<TWeakObjectPtr<AGimmickBase>> Pending = MoveTemp(PendingGimmickRestores);
    PendingGimmickRestores.Reset();
    if (!WorkingProgress) return;
	TGuardValue<bool> RestoreGuard(bRestoringGimmicks, true);
    Pending.StableSort([](const TWeakObjectPtr<AGimmickBase>& A, const TWeakObjectPtr<AGimmickBase>& B)
    {
        return (A.IsValid() ? A->GetRestorePriority() : -1) < (B.IsValid() ? B->GetRestorePriority() : -1);
    });
    // Snapshot records before callbacks so references survive any incidental save requests.
    TMap<AGimmickBase*, FProgressActorState> Records;
    for (const TWeakObjectPtr<AGimmickBase>& Weak : Pending)
    {
        AGimmickBase* Actor = Weak.Get();
        if (!IsValid(Actor)) continue;
        const FName MapName(*UGameplayStatics::GetCurrentLevelName(Actor, true));
        const FProgressMapState* Map = WorkingProgress->Maps.Find(MapName);
        const FProgressActorState* State = Map ? Map->Actors.Find(Actor->GetPersistentId()) : nullptr;
        if (State && State->State == TEXT("GimmickV1")) Records.Add(Actor, *State);
    }
    for (const TWeakObjectPtr<AGimmickBase>& Weak : Pending)
        if (Weak.IsValid()) if (const FProgressActorState* State = Records.Find(Weak.Get())) Weak->RestoreProgress(*State);
    for (const TWeakObjectPtr<AGimmickBase>& Weak : Pending)
        if (Weak.IsValid()) if (const FProgressActorState* State = Records.Find(Weak.Get())) Weak->FinishRestoreProgress(*State);
}

bool UProgressSaveSubsystem::RestoreCheckpointInPlace(APlayerBase* Player)
{
 if (!Player || !Player->IsDead() || !Player->HasAuthority() || IsSaving()) return false;
 // Boss combat has no complete in-place reset contract yet. Keep the safe travel path.
 for (TActorIterator<ABossMonster> It(Player->GetWorld()); It; ++It)
  if (It->GetBossState() != EBossState::Idle) return false;
 const UProgressSaveGame* Disk = ReadSlot(ActiveSlot);
 if (!Disk || Disk->MapPackage != UWorld::RemovePIEPrefix(Player->GetWorld()->GetOutermost()->GetName())) return false;
 // Missing records cannot safely undo an actor created after the checkpoint.
 const FProgressMapState* Map = Disk->Maps.Find(FName(*UGameplayStatics::GetCurrentLevelName(Player,true)));
 for (TActorIterator<AGimmickBase> It(Player->GetWorld()); It; ++It)
  if (It->ShouldPersist() && (!Map || !Map->Actors.Contains(It->GetPersistentId()))) return false;
 if (!ReloadActiveProgress()) return false;
 return RestoreLoadedCheckpointInPlace(Player);
}

bool UProgressSaveSubsystem::RestoreLoadedCheckpointInPlace(APlayerBase* Player)
{
 const double Start = FPlatformTime::Seconds();
 if (!Player || !Player->IsDead() || !WorkingProgress || !bRestorePending) return false;
 // Keep the guard through teleport/overlap callbacks and both gimmick restore passes.
 TGuardValue<bool> Guard(bRestoringGimmicks, true);
 for (TActorIterator<AProjectileBase> It(Player->GetWorld()); It; ++It) It->DestroyProjectile();
 for (TActorIterator<AGimmickBase> It(Player->GetWorld()); It; ++It) QueueGimmickRestore(*It);
 RestoreQueuedGimmicks();
 if (!Player->RespawnAtCheckpoint() || !RestorePlayerIfPending(Player)) return false;
 for (TActorIterator<ACorePickup> It(Player->GetWorld()); It; ++It)
  It->RestoreCheckpointConsumption(WorkingProgress->CorePickupStates.FindRef(It->GetPickupId()));
 UE_LOG(LogTemp, Log, TEXT("In-place checkpoint restore: %.1f ms"), (FPlatformTime::Seconds()-Start)*1000.0);
 return true;
}
