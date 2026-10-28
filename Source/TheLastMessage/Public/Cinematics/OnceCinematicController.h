#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/Boss/BossTypes.h"
#include "OnceCinematicController.generated.h"

class ALevelSequenceActor;
class ABossMonster;
class APlayerBase;
class APlayerController;
class APlayerHUD;
class UBrainComponent;

/** Place one per cinematic; IDs are stable within the map and save slot. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AOnceCinematicController : public AActor
{
	GENERATED_BODY()
public:
	AOnceCinematicController();
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") FName CinematicId;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") TObjectPtr<ALevelSequenceActor> SequenceActor;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") TObjectPtr<AActor> TriggerActor;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") bool bPlayOnBeginPlay = false;
	/** Entry scenes skip on loading even for older saves without cinematic history. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") bool bSkipOnLoad = false;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") TArray<TObjectPtr<AActor>> CleanupActors;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Cinematic") TObjectPtr<ABossMonster> Boss;
	UFUNCTION(BlueprintCallable, Category="Cinematic") bool RequestPlay();
	UFUNCTION(BlueprintCallable, Category="Cinematic") void SkipCinematic(APlayerController* RequestingController);
	UFUNCTION(BlueprintPure, Category="Cinematic") bool IsPlaying() const { return bPresenting; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void TryAutoPlay();
	void RestorePresentation();
	void ApplyCompletion();
	void SuspendBossCombat();
	void RestoreBossCombat();
	UFUNCTION() void TriggerEntered(AActor* OverlappedActor, AActor* OtherActor);
	UFUNCTION() void SequenceEnded();
	FTimerHandle StartupTimer;
	TWeakObjectPtr<APlayerBase> Player;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<APlayerHUD> PlayerHUD;
	TWeakObjectPtr<UBrainComponent> PausedBossBrain;
	EBossState PreviousBossState = EBossState::Idle;
	bool bBossCombatSuspended = false;
	bool bConsumed = false;
	bool bPresenting = false;
	bool bPreviousHidden = false;
	bool bPreviousInput = false;
	bool bPreviousHUD = true;
	int32 StartupAttempts = 0;
};
