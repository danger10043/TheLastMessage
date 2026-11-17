#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Boss/BossTypes.h"
#include "BossShieldComponent.generated.h"

class ALaserEmitter;

UCLASS()
class THELASTMESSAGE_API UBossShieldComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void ActivateShield(EBossPuzzleType PuzzleType);

    void DeactivateShield(bool bHideLaserActors = true);

    void StartPuzzleTimer();

    void StopPuzzleTimer();

    void HandlePuzzleTimeout();

    void ApplyFailureRecovery();

    bool CanApplyFailureRecovery() const;

    bool TryApplyFailureRecovery();

    bool IsShieldActive();

    float GetRemainingPuzzleTime();

    float GetPuzzleTimeLimit() const { return PuzzleTimeLimit; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY()
    bool bIsShieldActive = false;

    UPROPERTY()
    EBossPuzzleType CurrentPuzzleType = EBossPuzzleType::None;

    UPROPERTY(EditAnywhere, Category = "Boss|Puzzle|Timer",
        meta = (ClampMin = "0.1", Units = "s",
            DisplayName = "70% Puzzle Time Limit"))
    float Phase70PuzzleTimeLimit = 0.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Puzzle|Timer",
        meta = (ClampMin = "0.1", Units = "s",
            DisplayName = "30% Puzzle Time Limit"))
    float Phase30PuzzleTimeLimit = 0.f;

    UPROPERTY(Transient)
    float PuzzleTimeLimit = 0.f;

    UPROPERTY()
    float RemainingPuzzleTime = 0.f;

    UPROPERTY(EditDefaultsOnly)
    float FailureHealPercent = 0.1f;

    FTimerHandle PuzzleTimerHandle;

    UPROPERTY(EditInstanceOnly)
    TObjectPtr<ALaserEmitter> LaserEmitter = nullptr;
};