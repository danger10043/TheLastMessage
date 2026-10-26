#include "Save/ProgressSaveSubsystem.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString UProgressSaveSubsystem::GetSaveDirectory() const
{
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
	return SlotId > 0 && IFileManager::Get().FileExists(*GetSlotPath(SlotId));
}

bool UProgressSaveSubsystem::StartNewGame(int32 SlotId)
{
	if (ActiveSlot || bSaving || SlotId <= 0 || HasSave(SlotId)) return false;
	WorkingProgress = NewObject<UProgressSaveGame>(this);
	WorkingProgress->SlotId = SlotId;
	WorkingProgress->PlaythroughId = FGuid::NewGuid();
	ActiveSlot = SlotId;
	return true;
}

bool UProgressSaveSubsystem::ContinueGame(int32 SlotId)
{
	if (ActiveSlot || bSaving || SlotId <= 0) return false;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *GetSlotPath(SlotId))) return false;
	UProgressSaveGame* Loaded = Cast<UProgressSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!Loaded || Loaded->Version != UProgressSaveGame::CurrentVersion || Loaded->SlotId != SlotId
		|| !Loaded->PlaythroughId.IsValid()) return false;
	WorkingProgress = Loaded;
	ActiveSlot = SlotId;
	return true;
}

bool UProgressSaveSubsystem::EndSession()
{
	if (bSaving) return false;
	WorkingProgress = nullptr;
	ActiveSlot = 0;
	return true;
}

bool UProgressSaveSubsystem::SaveActiveProgress()
{
	if (bSaving || !WorkingProgress || ActiveSlot <= 0 || WorkingProgress->SlotId != ActiveSlot) return false;
	WorkingProgress->SavedAt = FDateTime::UtcNow();
	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(WorkingProgress, Bytes)) return false;
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
				Self->OnSaveFinished.Broadcast(bSuccess);
			}
		});
	});
	return true;
}
