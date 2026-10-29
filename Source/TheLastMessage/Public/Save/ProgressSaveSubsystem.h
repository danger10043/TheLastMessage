#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/ProgressSaveGame.h"
#include "ProgressSaveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProgressSaveFinished, bool, bSuccess);

USTRUCT(BlueprintType)
struct FProgressSlotInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bExists = false;
	UPROPERTY(BlueprintReadOnly) bool bCanLoad = false;
	UPROPERTY(BlueprintReadOnly) bool bHasCheckpoint = false;
	UPROPERTY(BlueprintReadOnly) FDateTime SavedAt;
	UPROPERTY(BlueprintReadOnly) FString MapPackage;
};

/** Session owns the slot. Save callers cannot specify or change its destination. */
UCLASS()
class THELASTMESSAGE_API UProgressSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	/** Begin only from the title/menu. Replacing an occupied slot requires explicit confirmation. */
	UFUNCTION(BlueprintCallable, Category="Save") bool StartNewGame(int32 SlotId, bool bOverwriteExisting = false);
	UFUNCTION(BlueprintPure, Category="Save") FProgressSlotInfo GetSlotInfo(int32 SlotId) const;
	UFUNCTION(BlueprintCallable, Category="Save") bool ContinueGame(int32 SlotId);
	/** Re-read the current slot without permitting a slot switch; failure preserves the session. */
	UFUNCTION(BlueprintCallable, Category="Save") bool ReloadActiveProgress();
	/** Explicitly return to menu before selecting another playthrough. Fails during a write. */
	UFUNCTION(BlueprintCallable, Category="Save") bool EndSession();
	/** Both checkpoint and autosave must call this, after updating the checkpoint snapshot. */
	UFUNCTION(BlueprintCallable, Category="Save") bool SaveActiveProgress();
	bool SaveCheckpoint(class APlayerBase* Player, class APlayerCheckpoint* Checkpoint);
	bool RestorePlayerIfPending(APlayerBase* Player);
	bool RestoreCheckpointInPlace(APlayerBase* Player);
	void QueueGimmickRestore(class AGimmickBase* Gimmick);
	bool IsRestoringGimmicks() const { return bRestoringGimmicks; }
	void ApplyCheckpointState(APlayerCheckpoint* Checkpoint) const;
	bool IsRestoringProgress() const { return bRestorePending || bRestoring; }
	bool IsLoadedSession() const { return bLoadedSession; }
	bool IsLoadedMap(FName Map) const { return bLoadedSession && LoadedMap == Map; }
	bool HasPlayedCinematic(FName Map, FName Cinematic) const;
	bool MarkCinematicPlayed(FName Map, FName Cinematic);
	UFUNCTION(BlueprintPure, Category="Save") int32 GetActiveSlot() const { return ActiveSlot; }
	UFUNCTION(BlueprintPure, Category="Save") bool IsSaving() const { return bSaving || PendingCheckpointSnapshot != nullptr || bCinematicHistoryDirty; }
	UFUNCTION(BlueprintPure, Category="Save") bool HasSave(int32 SlotId) const;
	UFUNCTION(BlueprintPure, Category="Save") FString GetSaveDirectory() const;
	UPROPERTY(BlueprintAssignable, Category="Save") FProgressSaveFinished OnSaveFinished;
	UPROPERTY(BlueprintAssignable, Category="Save") FProgressSaveFinished OnCheckpointSaveFinished;
	// Mutable working state only. Disk writes take an independent serialized snapshot.
	UProgressSaveGame* GetWorkingProgress() const { return WorkingProgress; }
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FProgressSlotFlowTest;
	friend class FCheckpointSaveFlowTest;
	friend class FCheckpointRestoreStartupTest;
	friend class FCinematicProgressTest;
	friend class FCinematicControllerTest;
	FString AutomationSaveDirectory;
#endif
	bool RestoreLoadedCheckpointInPlace(APlayerBase* Player);
	UProgressSaveGame* ReadSlot(int32 SlotId) const;
	FString GetSlotPath(int32 SlotId) const;
	UPROPERTY(Transient) TObjectPtr<UProgressSaveGame> WorkingProgress;
	int32 ActiveSlot = 0;
	bool bSaving = false;
	bool bCheckpointWrite = false;
	bool bRestorePending = false;
	bool bRestoring = false;
	bool bLoadedSession = false;
	FName LoadedMap;
	bool bCinematicHistoryDirty = false;
	UPROPERTY(Transient) TObjectPtr<UProgressSaveGame> PendingCheckpointSnapshot;
	void WriteCheckpointSnapshot(UProgressSaveGame* Snapshot);
	TArray<TWeakObjectPtr<class AGimmickBase>> PendingGimmickRestores;
	void RestoreQueuedGimmicks();
	bool bRestoringGimmicks = false;
};
