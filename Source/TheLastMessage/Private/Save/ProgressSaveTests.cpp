#if WITH_DEV_AUTOMATION_TESTS
#include "Save/ProgressSaveSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressSlotFlowTest, "TheLastMessage.Save.SlotFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProgressSlotFlowTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UProgressSaveSubsystem> Saves(NewObject<UProgressSaveSubsystem>(Instance.Get()));
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/ProgressSlots") / FGuid::NewGuid().ToString());
	Saves->AutomationSaveDirectory = Directory;
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path1 = Directory / TEXT("Progress_1.sav"), Path2 = Directory / TEXT("Progress_2.sav"), Path3 = Directory / TEXT("Progress_3.sav");
	UProgressSaveGame* Unsupported = NewObject<UProgressSaveGame>();
	Unsupported->Version = UProgressSaveGame::CurrentVersion + 1;
	Unsupported->SlotId = 1;
	Unsupported->PlaythroughId = FGuid::NewGuid();
	Unsupported->MapPackage = TEXT("/Game/Test_Jihong/Prototype/Lv_Factory_Metroidvania");
	TArray<uint8> Sentinel;
	UGameplayStatics::SaveGameToMemory(Unsupported, Sentinel);
	FFileHelper::SaveArrayToFile(Sentinel, *Path1);
	FFileHelper::SaveArrayToFile(Sentinel, *Path3);
	auto WaitForWrite = [&]()
	{
		const double Deadline = FPlatformTime::Seconds() + 10;
		while (Saves->IsSaving() && FPlatformTime::Seconds() < Deadline)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FPlatformProcess::Sleep(0.005f);
		}
		return TestFalse(TEXT("Write completes"), Saves->IsSaving());
	};
	TestFalse(TEXT("Slot zero rejected"), Saves->StartNewGame(0));
	TestFalse(TEXT("Fourth slot rejected"), Saves->StartNewGame(4));
	TestTrue(TEXT("New game selects slot two"), Saves->StartNewGame(2));
	TestFalse(TEXT("Cannot switch slot during a playthrough"), Saves->StartNewGame(1, true));
	TestFalse(TEXT("Cannot load another slot during a playthrough"), Saves->ContinueGame(3));
	UProgressSaveGame* Progress = Saves->GetWorkingProgress();
	Progress->MapPackage = TEXT("/Game/Test_Jihong/Prototype/Lv_Factory_Metroidvania");
	Progress->Maps.FindOrAdd(TEXT("Factory")).ExploredRooms.Add(TEXT("RoomA"));
	Progress->CheckpointId = FGuid::NewGuid();
	const FGuid FirstId = Progress->PlaythroughId;
	TestTrue(TEXT("Initial save accepted"), Saves->SaveActiveProgress());
	TestFalse(TEXT("Duplicate save rejected"), Saves->SaveActiveProgress());
	TestFalse(TEXT("Cannot end session during save"), Saves->EndSession());
	if (!WaitForWrite()) return false;
	TestTrue(TEXT("Slot metadata is loadable"), Saves->GetSlotInfo(2).bCanLoad);
	TestTrue(TEXT("Stored timestamp present"), Saves->GetSlotInfo(2).SavedAt.GetTicks() > 0);
	TestFalse(TEXT("Unsupported version cannot be loaded"), Saves->GetSlotInfo(1).bCanLoad);
	TestTrue(TEXT("End session"), Saves->EndSession());
	TestTrue(TEXT("Continue selected slot"), Saves->ContinueGame(2));
	TestTrue(TEXT("Continue arms checkpoint restoration"), Saves->IsRestoringProgress());
	TestEqual(TEXT("Continue keeps selected slot"), Saves->GetActiveSlot(), 2);
	TestTrue(TEXT("Playthrough preserved"), Saves->GetWorkingProgress()->PlaythroughId == FirstId);
	TestTrue(TEXT("Exploration survives disk round trip"), Saves->GetWorkingProgress()->Maps.FindChecked(TEXT("Factory")).ExploredRooms.Contains(TEXT("RoomA")));
	Saves->EndSession();
	TestFalse(TEXT("Overwrite needs confirmation"), Saves->StartNewGame(2));
	TestTrue(TEXT("Confirmed replacement accepted"), Saves->StartNewGame(2, true));
	TestTrue(TEXT("Replacement clears exploration"), Saves->GetWorkingProgress()->Maps.IsEmpty());
	TestTrue(TEXT("New playthrough identity"), Saves->GetWorkingProgress()->PlaythroughId != FirstId);
	TestTrue(TEXT("Old save remains before replacement write"), Saves->ReadSlot(2)->PlaythroughId == FirstId);
	Saves->GetWorkingProgress()->MapPackage = Progress->MapPackage;
	TestTrue(TEXT("Replacement write accepted"), Saves->SaveActiveProgress());
	if (!WaitForWrite()) return false;
	TestTrue(TEXT("Replacement updates only selected file"), Saves->ReadSlot(2)->PlaythroughId != FirstId);
	TArray<uint8> Bytes;
	FFileHelper::LoadFileToArray(Bytes, *Path1); TestTrue(TEXT("Slot one untouched"), Bytes == Sentinel);
	FFileHelper::LoadFileToArray(Bytes, *Path3); TestTrue(TEXT("Slot three untouched"), Bytes == Sentinel);
	TestFalse(TEXT("No loadable temporary save"), IFileManager::Get().FileExists(*(Path2 + TEXT(".pending"))));
	Saves->EndSession();
	// Only this test's three files and empty directories are removed.
	IFileManager::Get().Delete(*Path1); IFileManager::Get().Delete(*Path2); IFileManager::Get().Delete(*Path3);
	IFileManager::Get().DeleteDirectory(*Directory, false, false);
	return !HasAnyErrors();
}
#endif
