#include "UI/StartMenuHUD.h"
#include "UI/StartMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Save/ProgressSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

AStartMenuHUD::AStartMenuHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStartMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	Menu = CreateWidget<UStartMenuWidget>(PC, MenuClass);
	if (!Menu) return;
	Menu->OnExitRequested.AddUniqueDynamic(this, &AStartMenuHUD::ExitGame);
	Menu->OnSlotSelected.AddUniqueDynamic(this, &AStartMenuHUD::SelectSlot);
	Menu->AddToPlayerScreen();
	FInputModeUIOnly Input;
	Input.SetWidgetToFocus(Menu->TakeWidget());
	Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Input);
	PC->SetShowMouseCursor(true);
	Menu->ShowMainMenu();
	if (UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>())
	{
		Saves->EndSession();
		Saves->OnSaveFinished.AddUniqueDynamic(this, &AStartMenuHUD::InitialSaveFinished);
	}
	RefreshSlots();
}

void AStartMenuHUD::RefreshSlots()
{
	UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (!Menu || !Saves) return;
	for (int32 Slot = 1; Slot <= 3; ++Slot)
	{
		const FProgressSlotInfo Info = Saves->GetSlotInfo(Slot);
		SlotExists[Slot - 1] = Info.bExists;
		const FText Description = !Info.bExists ? FText::GetEmpty() : Info.bCanLoad
			? FText::AsDateTime(Info.SavedAt)
			: NSLOCTEXT("StartMenu", "Unreadable", "불러올 수 없는 저장 파일");
		Menu->SetSlotPresentation(Slot, Info.bExists, Description, Info.bCanLoad);
	}
}

void AStartMenuHUD::SelectSlot(int32 SlotId, bool bNewGame)
{
	if (bStartingGame || !Menu || SlotId < 1 || SlotId > 3) return;
	UProgressSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>();
	if (!Saves || Saves->IsSaving())
	{
		Menu->ShowError(NSLOCTEXT("StartMenu", "WaitSave", "저장이 끝난 뒤 다시 시도해 주세요.")); return;
	}
	if (Saves->GetActiveSlot() != 0)
	{
		Menu->ShowError(NSLOCTEXT("StartMenu", "SessionActive", "이미 시작한 게임이 있습니다.")); return;
	}
	if (bNewGame)
	{
		const FString Package = NewGameMap.ToSoftObjectPath().GetLongPackageName();
		if (!FPackageName::DoesPackageExist(Package))
		{
			Menu->ShowError(NSLOCTEXT("StartMenu", "MissingMap", "게임 맵을 찾을 수 없습니다.")); return;
		}
		// A newly appeared file must be presented and confirmed before replacement.
		if (Saves->HasSave(SlotId) && !SlotExists[SlotId - 1])
		{
			RefreshSlots();
			Menu->ShowError(NSLOCTEXT("StartMenu", "SlotChanged", "슬롯 정보가 변경되었습니다. 다시 선택해 주세요.")); return;
		}
		if (!Saves->StartNewGame(SlotId, SlotExists[SlotId - 1]))
		{
			Menu->ShowError(NSLOCTEXT("StartMenu", "StartFailed", "새 게임을 시작하지 못했습니다.")); return;
		}
		Saves->GetWorkingProgress()->MapPackage = Package;
		bStartingGame = true;
		Menu->SetBusy(true);
		if (!Saves->SaveActiveProgress()) InitialSaveFinished(false);
	}
	else
	{
		if (!Saves->ContinueGame(SlotId))
		{
			RefreshSlots();
			Menu->ShowError(NSLOCTEXT("StartMenu", "LoadFailed", "저장 파일을 불러오지 못했습니다.")); return;
		}
		bStartingGame = true;
		Menu->SetBusy(true);
		TravelToProgress();
	}
}

void AStartMenuHUD::InitialSaveFinished(bool bSuccess)
{
	if (!bStartingGame) return;
	if (bSuccess) { TravelToProgress(); return; }
	bStartingGame = false;
	GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->EndSession();
	RefreshSlots();
	if (Menu) Menu->ShowError(NSLOCTEXT("StartMenu", "WriteFailed", "저장하지 못했습니다. 저장 폴더를 확인한 뒤 다시 시도해 주세요."));
}

void AStartMenuHUD::TravelToProgress()
{
	const UProgressSaveGame* Progress = GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->GetWorkingProgress();
	if (Progress) UGameplayStatics::OpenLevel(this, FName(*Progress->MapPackage));
}

void AStartMenuHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetGameInstance())
		GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>()->OnSaveFinished.RemoveDynamic(this, &AStartMenuHUD::InitialSaveFinished);
	if (Menu) Menu->RemoveFromParent();
	Menu = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AStartMenuHUD::ExitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

AStartMenuGameMode::AStartMenuGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AStartMenuHUD::StaticClass();
}
