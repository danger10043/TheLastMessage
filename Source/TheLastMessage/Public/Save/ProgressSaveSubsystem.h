#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/ProgressSaveGame.h"
#include "ProgressSaveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProgressSaveFinished, bool, bSuccess);

/** Session owns the slot. Save callers cannot specify or change its destination. */
UCLASS()
class THELASTMESSAGE_API UProgressSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	/** Begin only from the title/menu. Existing files are never overwritten by StartNewGame. */
	UFUNCTION(BlueprintCallable, Category="Save") bool StartNewGame(int32 SlotId);
	UFUNCTION(BlueprintCallable, Category="Save") bool ContinueGame(int32 SlotId);
	/** Explicitly return to menu before selecting another playthrough. Fails during a write. */
	UFUNCTION(BlueprintCallable, Category="Save") bool EndSession();
	/** Both checkpoint and autosave must call this, after updating the checkpoint snapshot. */
	UFUNCTION(BlueprintCallable, Category="Save") bool SaveActiveProgress();
	UFUNCTION(BlueprintPure, Category="Save") int32 GetActiveSlot() const { return ActiveSlot; }
	UFUNCTION(BlueprintPure, Category="Save") bool IsSaving() const { return bSaving; }
	UFUNCTION(BlueprintPure, Category="Save") bool HasSave(int32 SlotId) const;
	UFUNCTION(BlueprintPure, Category="Save") FString GetSaveDirectory() const;
	UPROPERTY(BlueprintAssignable, Category="Save") FProgressSaveFinished OnSaveFinished;
	// Mutable working state only. Disk writes take an independent serialized snapshot.
	UProgressSaveGame* GetWorkingProgress() const { return WorkingProgress; }
private:
	FString GetSlotPath(int32 SlotId) const;
	UPROPERTY(Transient) TObjectPtr<UProgressSaveGame> WorkingProgress;
	int32 ActiveSlot = 0;
	bool bSaving = false;
};
