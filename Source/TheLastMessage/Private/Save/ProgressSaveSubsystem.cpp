#include "Save/ProgressSaveSubsystem.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"

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
		Info.SavedAt = Save->SavedAt;
		Info.MapPackage = Save->MapPackage;
	}
	return Info;
}

bool UProgressSaveSubsystem::StartNewGame(int32 SlotId, bool bOverwriteExisting)
{
	if (ActiveSlot || bSaving || SlotId < 1 || SlotId > 3 || (HasSave(SlotId) && !bOverwriteExisting)) return false;
	WorkingProgress = NewObject<UProgressSaveGame>(this);
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
