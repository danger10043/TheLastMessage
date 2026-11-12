#include "Gimmick/Flag/RemoteGatePresenter.h"
#include "Gimmick/Flag/WorldFlagSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/SideScrollingCameraManager.h"
#include "Character/Player/PlayerBase.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Map/RoomStreamingSubsystem.h"

bool URemoteGatePresenter::ShouldCreateSubsystem(UObject* Outer) const
{
	const URemoteGateSettings* Settings = GetDefault<URemoteGateSettings>();
	return Settings && !Settings->RemoteGates.IsEmpty() && Super::ShouldCreateSubsystem(Outer);
}

bool URemoteGatePresenter::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URemoteGatePresenter::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorldFlagSubsystem* Flags = UWorldFlagSubsystem::Get(this))
		Flags->DeferHandler.BindUObject(this, &URemoteGatePresenter::HandleFlagDefer);
}

void URemoteGatePresenter::Deinitialize()
{
	if (Phase != EPhase::Idle) Finish(false);
	if (UWorldFlagSubsystem* Flags = UWorldFlagSubsystem::Get(this))
		if (Flags->DeferHandler.IsBoundToObject(this)) Flags->DeferHandler.Unbind();
	Super::Deinitialize();
}

bool URemoteGatePresenter::IsTickable() const
{
	return Phase != EPhase::Idle && Super::IsTickable();
}

TStatId URemoteGatePresenter::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URemoteGatePresenter, STATGROUP_Tickables);
}

bool URemoteGatePresenter::HandleFlagDefer(FGameplayTag Flag, bool bValue)
{
	if (!bValue || Phase != EPhase::Idle || !Flag.IsValid()) return false;

	const FRemoteGateEntry* Entry = GetDefault<URemoteGateSettings>()->RemoteGates.FindByPredicate(
		[&Flag](const FRemoteGateEntry& Candidate) { return Candidate.Flag == Flag; });
	if (!Entry) return false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	if (!Player || Player->IsDead() || !PC->PlayerCameraManager) return false;

	// 문이 가까이 있으면 연출 없이 그냥 열린다.
	const FVector Location = Player->GetActorLocation();
	const FVector2D Delta(Location.X - Entry->FocusLocation.X, Location.Z - Entry->FocusLocation.Z);
	if (Delta.Size() <= Entry->MinPlayerDistance) return false;

	Begin(*Entry, PC);
	return true;
}

void URemoteGatePresenter::Begin(const FRemoteGateEntry& Entry, APlayerController* PC)
{
	Active = Entry;
	Controller = PC;
	Phase = EPhase::FadingOut;
	PhaseTime = 0.f;
	LoadedTime = 0.f;

	SetInputLocked(true);
	FadeTo(0.f, 1.f);
	if (URoomStreamingSubsystem* Rooms = GetWorld()->GetSubsystem<URoomStreamingSubsystem>())
	{
		Rooms->PinRooms(Active.RoomIds);
		bPinned = true;
	}
}

void URemoteGatePresenter::FadeTo(float From, float To) const
{
	if (APlayerController* PC = Controller.Get())
		if (PC->PlayerCameraManager)
			PC->PlayerCameraManager->StartCameraFade(From, To, GetDefault<URemoteGateSettings>()->FadeSeconds, FLinearColor::Black, false, To >= 1.f);
}

void URemoteGatePresenter::SetInputLocked(bool bLocked)
{
	if (bInputLocked == bLocked) return;
	APlayerController* PC = Controller.Get();
	if (!PC) return;
	bInputLocked = bLocked;
	PC->SetIgnoreMoveInput(bLocked);
	PC->SetIgnoreLookInput(bLocked);
	if (APawn* Pawn = PC->GetPawn())
	{
		if (bLocked)
		{
			if (ACharacter* Character = Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
			Pawn->DisableInput(PC);
		}
		else
		{
			Pawn->EnableInput(PC);
		}
	}
}

void URemoteGatePresenter::ShowFocusCamera()
{
	APlayerController* PC = Controller.Get();
	UWorld* World = GetWorld();
	if (!PC || !World) return;

	// 플레이어 카메라와 같은 방식: 옆(-Y 방향)에서 Zoom 거리만큼 떨어져 비춘다.
	float Zoom = 2000.f;
	if (const ASideScrollingCameraManager* Manager = Cast<ASideScrollingCameraManager>(PC->PlayerCameraManager)) Zoom = Manager->CurrentZoom;
	const FVector Location(Active.FocusLocation.X, Active.FocusLocation.Y + Zoom, Active.FocusLocation.Z);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(Location, FRotator(0.f, -90.f, 0.f), Params);
	if (!Camera) return;
	Camera->GetCameraComponent()->SetFieldOfView(65.f);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	FocusCamera = Camera;
	PC->SetViewTarget(Camera);
}

void URemoteGatePresenter::RestorePlayerView()
{
	if (APlayerController* PC = Controller.Get())
		if (APawn* Pawn = PC->GetPawn()) PC->SetViewTarget(Pawn);
	if (ACameraActor* Camera = FocusCamera.Get()) Camera->Destroy();
	FocusCamera.Reset();
}

void URemoteGatePresenter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Phase == EPhase::Idle) return;

	const URemoteGateSettings* Settings = GetDefault<URemoteGateSettings>();
	APlayerController* PC = Controller.Get();
	const APlayerBase* Player = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	if (!Player || Player->IsDead())
	{
		// 연출 중 플레이어가 죽거나 사라지면 정리하고 끝낸다. 문은 플래그대로 열린다.
		Finish(true);
		return;
	}

	PhaseTime += DeltaTime;
	switch (Phase)
	{
	case EPhase::FadingOut:
		if (PhaseTime >= Settings->FadeSeconds) { Phase = EPhase::Loading; PhaseTime = 0.f; LoadedTime = 0.f; }
		break;

	case EPhase::Loading:
	{
		const URoomStreamingSubsystem* Rooms = GetWorld()->GetSubsystem<URoomStreamingSubsystem>();
		const bool bLoaded = !Rooms || Rooms->AreRoomsLoaded(Active.RoomIds);
		LoadedTime = bLoaded ? LoadedTime + DeltaTime : 0.f;
		if (LoadedTime >= Settings->SettleSeconds || PhaseTime >= Settings->LoadTimeoutSeconds)
		{
			ShowFocusCamera();
			FadeTo(1.f, 0.f);
			Phase = EPhase::FadingIn;
			PhaseTime = 0.f;
		}
		break;
	}

	case EPhase::FadingIn:
		if (PhaseTime >= Settings->FadeSeconds)
		{
			// 이제 문이 열리는 연출이 보인다.
			if (UWorldFlagSubsystem* Flags = UWorldFlagSubsystem::Get(this)) Flags->EndFlagDeferral(Active.Flag);
			Phase = EPhase::Holding;
			PhaseTime = 0.f;
		}
		break;

	case EPhase::Holding:
		if (PhaseTime >= Active.HoldSeconds)
		{
			FadeTo(0.f, 1.f);
			Phase = EPhase::FadingBack;
			PhaseTime = 0.f;
		}
		break;

	case EPhase::FadingBack:
		if (PhaseTime >= Settings->FadeSeconds)
		{
			RestorePlayerView();
			if (bPinned)
			{
				if (URoomStreamingSubsystem* Rooms = GetWorld()->GetSubsystem<URoomStreamingSubsystem>()) Rooms->UnpinRooms(Active.RoomIds);
				bPinned = false;
			}
			FadeTo(1.f, 0.f);
			Phase = EPhase::Returning;
			PhaseTime = 0.f;
		}
		break;

	case EPhase::Returning:
		if (PhaseTime >= Settings->FadeSeconds) Finish(true);
		break;

	default:
		break;
	}
}

void URemoteGatePresenter::Finish(bool bNotify)
{
	RestorePlayerView();
	if (bPinned)
	{
		if (URoomStreamingSubsystem* Rooms = GetWorld() ? GetWorld()->GetSubsystem<URoomStreamingSubsystem>() : nullptr) Rooms->UnpinRooms(Active.RoomIds);
		bPinned = false;
	}
	if (APlayerController* PC = Controller.Get())
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
	SetInputLocked(false);
	if (UWorldFlagSubsystem* Flags = UWorldFlagSubsystem::Get(this)) Flags->EndFlagDeferral(Active.Flag, bNotify);
	Phase = EPhase::Idle;
}
