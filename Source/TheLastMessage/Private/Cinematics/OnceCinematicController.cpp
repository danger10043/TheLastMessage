#include "Cinematics/OnceCinematicController.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossPatternComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Save/ProgressSaveSubsystem.h"
#include "UI/PlayerHUD.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/SideScrollingPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"

AOnceCinematicController::AOnceCinematicController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AOnceCinematicController::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;
	// A nearby checkpoint can put the player inside the AI detection radius before
	// they enter the boss intro trigger. Keep its health bar gated until completion.
	if (IsValid(Boss)) Boss->SetBossUIVisible(false);
	if (TriggerActor) TriggerActor->OnActorBeginOverlap.AddUniqueDynamic(this, &AOnceCinematicController::TriggerEntered);
	if (bPlayOnBeginPlay || TriggerActor) GetWorldTimerManager().SetTimerForNextTick(this, &AOnceCinematicController::TryAutoPlay);
}

namespace
{
	bool ShouldSkipCinematic(const UProgressSaveSubsystem* Saves, FName Map, FName Id,
		bool bSkipOnLoad, bool bReplayOnDeathCheckpoint)
	{
		if (!Saves) return false;
		if (bReplayOnDeathCheckpoint && Saves->IsDeathCheckpointRetry(Map))
		{
			return Saves->HasPlayedCinematicOnRetry(Id);
		}
		return (bSkipOnLoad && Saves->IsLoadedMap(Map)) || Saves->HasPlayedCinematic(Map, Id);
	}
}

ABossMonster* AOnceCinematicController::GetBossForDeathRetry() const
{
	return bReplayOnDeathCheckpoint && bStartBossCombatOnCompletion ? Boss.Get() : nullptr;
}

bool AOnceCinematicController::ResetForDeathCheckpoint()
{
	ABossMonster* RetryBoss = GetBossForDeathRetry();
	if (!HasAuthority() || !IsValid(RetryBoss)) return false;

	GetWorldTimerManager().ClearTimer(StartupTimer);
	if (ULevelSequencePlayer* Sequence = IsValid(SequenceActor)
		? SequenceActor->GetSequencePlayer() : nullptr)
	{
		Sequence->OnFinished.RemoveDynamic(this, &AOnceCinematicController::SequenceEnded);
		Sequence->OnStop.RemoveDynamic(this, &AOnceCinematicController::SequenceEnded);
		if (Sequence->IsPlaying() || Sequence->IsPaused()) Sequence->Stop();
	}

	RestorePresentation();
	if (!RetryBoss->ResetBossForCheckpoint(true)) return false;
	RetryBoss->SetBossUIVisible(false);
	bConsumed = false;
	StartupAttempts = 0;
	GetWorldTimerManager().SetTimerForNextTick(this, &AOnceCinematicController::TryAutoPlay);
	return true;
}

void AOnceCinematicController::TryAutoPlay()
{
	if (bConsumed) return;

	if (bStartBossCombatOnCompletion && !CinematicId.IsNone())
	{
		UProgressSaveSubsystem* Saves = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>() : nullptr;
		if (Saves && Saves->IsRestoringProgress())
		{
			GetWorldTimerManager().SetTimer(
				StartupTimer, this, &AOnceCinematicController::TryAutoPlay, 0.05f, false);
			return;
		}

		const FName Map(*UGameplayStatics::GetCurrentLevelName(this, true));
		if (ShouldSkipCinematic(Saves, Map, CinematicId, bSkipOnLoad, bReplayOnDeathCheckpoint))
		{
			bConsumed = true;
			Saves->MarkCinematicPlayed(Map, CinematicId);
			ApplyCompletion();
			return;
		}
	}

	if (!bPlayOnBeginPlay && TriggerActor
		&& !TriggerActor->IsOverlappingActor(UGameplayStatics::GetPlayerPawn(this, 0))) return;
	if (RequestPlay() || bConsumed) return;
	if (++StartupAttempts < 100)
	{
		GetWorldTimerManager().SetTimer(
			StartupTimer, this, &AOnceCinematicController::TryAutoPlay, 0.05f, false);
	}
}

void AOnceCinematicController::TriggerEntered(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor == UGameplayStatics::GetPlayerPawn(this, 0)) TryAutoPlay();
}

bool AOnceCinematicController::RequestPlay()
{
	if (!HasAuthority() || bConsumed || CinematicId.IsNone()) return false;
	UProgressSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProgressSaveSubsystem>() : nullptr;
	const FName Map(*UGameplayStatics::GetCurrentLevelName(this, true));
	if (ShouldSkipCinematic(Saves, Map, CinematicId, bSkipOnLoad, bReplayOnDeathCheckpoint))
	{
		bConsumed = true;
		Saves->MarkCinematicPlayed(Map, CinematicId);
		ApplyCompletion();
		return false;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APlayerBase* Pawn = PC ? Cast<APlayerBase>(PC->GetPawn()) : nullptr;
	ULevelSequencePlayer* Sequence = IsValid(SequenceActor)
		? SequenceActor->GetSequencePlayer() : nullptr;
	if (!IsValid(PC) || !IsValid(Pawn) || Pawn->IsDead()
		|| !Pawn->HasActorBegunPlay() || !IsValid(Sequence)
		|| !IsValid(Sequence->GetSequence()) || Sequence->IsPlaying() || Sequence->IsPaused()
		|| Sequence->GetDuration().AsSeconds() <= 0.0)
	{
		return false;
	}
	// Defer until the save system has restored the player, so a scene cannot race the load teleport.
	if (Saves && Saves->IsRestoringProgress()) return false;
	bConsumed = true;
	if (Saves)
	{
		if (bReplayOnDeathCheckpoint && Saves->IsDeathCheckpointRetry(Map))
		{
			Saves->MarkCinematicPlayedOnRetry(CinematicId);
		}
		Saves->MarkCinematicPlayed(Map, CinematicId);
	}
	Player = Pawn; Controller = PC; PlayerHUD = Cast<APlayerHUD>(PC->GetHUD());
	bPreviousHidden = Pawn->IsHidden(); bPreviousInput = Pawn->InputEnabled();
	bPreviousHUD = PlayerHUD.IsValid() ? PlayerHUD->bShowHUD : true;
	bPresenting = true;
	Pawn->CancelWeaponInputsForUI();
	Pawn->SetActorHiddenInGame(bPreviousHidden || bHidePlayerDuringCinematic);
	Pawn->DisableInput(PC);
	if (PlayerHUD.IsValid()) PlayerHUD->SetPlayerUIVisible(false);
	SuspendBossCombat();
	if (IsValid(Boss))
	{
		if (ASideScrollingPlayerController* BossPC = Cast<ASideScrollingPlayerController>(PC))
		{
			BossPC->ClientSetBossSequenceInputBlocked(Boss.Get(), true);
		}
	}
	if (Boss) Boss->SetBossUIVisible(false);
	if (bReplayOnDeathCheckpoint)
	{
		Sequence->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
			Sequence->GetStartTime().Time, EUpdatePositionMethod::Jump));
	}
	Sequence->OnFinished.AddUniqueDynamic(this, &AOnceCinematicController::SequenceEnded);
	Sequence->OnStop.AddUniqueDynamic(this, &AOnceCinematicController::SequenceEnded);
	Sequence->Play();
	return true;
}

void AOnceCinematicController::SkipCinematic(APlayerController* RequestingController)
{
	if (!HasAuthority() || !bPresenting || !RequestingController || Controller.Get() != RequestingController) return;
	if (ULevelSequencePlayer* Sequence = SequenceActor ? SequenceActor->GetSequencePlayer() : nullptr)
	{
		// Evaluate the final scene state and release sequence camera control before cleanup.
		Sequence->GoToEndAndStop();
	}
	SequenceEnded();
}

void AOnceCinematicController::RestorePresentation()
{
	if (!bPresenting) return;
	bPresenting = false;
	RestoreBossCombat();
	if (Player.IsValid())
	{
		Player->SetActorHiddenInGame(bPreviousHidden);
		if (bPreviousInput && Controller.IsValid()) Player->EnableInput(Controller.Get());
	}
	if (PlayerHUD.IsValid()) PlayerHUD->SetPlayerUIVisible(bPreviousHUD);
	if (ASideScrollingPlayerController* BossPC =
		Cast<ASideScrollingPlayerController>(Controller.Get()))
	{
		BossPC->ClientSetBossSequenceInputBlocked(Boss.Get(), false);
	}
}

void AOnceCinematicController::SuspendBossCombat()
{
	if (!IsValid(Boss) || Boss->GetBossState() == EBossState::Dead) return;
	PreviousBossState = Boss->GetBossState();
	bBossCombatSuspended = true;
	if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent(); Brain && Brain->IsRunning() && !Brain->IsPaused())
		{
			PausedBossBrain = Brain;
			Brain->PauseLogic(TEXT("Boss intro cinematic"));
		}
		AI->StopMovement();
	}
	if (UBossPatternComponent* Pattern = Boss->FindComponentByClass<UBossPatternComponent>()) Pattern->CancelCurrentAttack();
	Boss->SetBossState(EBossState::Cinematic);
	Boss->GetCharacterMovement()->StopMovementImmediately();
}

void AOnceCinematicController::RestoreBossCombat()
{
	if (!bBossCombatSuspended) return;
	bBossCombatSuspended = false;
	if (IsValid(Boss) && Boss->GetBossState() == EBossState::Cinematic)
	{
		// A cancelled attack cannot be resumed halfway through its old state.
		Boss->SetBossState(PreviousBossState == EBossState::Attacking ? EBossState::Combat : PreviousBossState);
	}
	if (PausedBossBrain.IsValid() && IsValid(Boss)
		&& Boss->GetBossState() != EBossState::Dead && Boss->GetBossState() != EBossState::ShieldPuzzle
		&& Boss->GetBossState() != EBossState::Cinematic)
	{
		PausedBossBrain->ResumeLogic(TEXT("Boss intro cinematic finished"));
	}
	PausedBossBrain.Reset();
}

void AOnceCinematicController::ApplyCompletion()
{
	if (bStartBossCombatOnCompletion && IsValid(Boss))
	{
		Boss->StartBossCombatAfterEntrance();
	}

	if (Boss) Boss->SetBossUIVisible(true);
	for (AActor* Actor : CleanupActors) if (IsValid(Actor) && Actor != this) Actor->Destroy();
}

void AOnceCinematicController::SequenceEnded()
{
	if (ASideScrollingPlayerController* BossPC =
		Cast<ASideScrollingPlayerController>(Controller.Get()))
	{
		BossPC->ClientSetBossSequenceInputBlocked(Boss.Get(), false);
	}
	if (!bPresenting) return;
	RestorePresentation();
	ApplyCompletion();
}

void AOnceCinematicController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartupTimer);
	if (IsValid(TriggerActor)) TriggerActor->OnActorBeginOverlap.RemoveDynamic(this, &AOnceCinematicController::TriggerEntered);
	if (SequenceActor && SequenceActor->GetSequencePlayer())
	{
		SequenceActor->GetSequencePlayer()->OnFinished.RemoveDynamic(this, &AOnceCinematicController::SequenceEnded);
		SequenceActor->GetSequencePlayer()->OnStop.RemoveDynamic(this, &AOnceCinematicController::SequenceEnded);
		if (bPresenting) SequenceActor->GetSequencePlayer()->Stop();
	}
	RestorePresentation();
	Super::EndPlay(EndPlayReason);
}
