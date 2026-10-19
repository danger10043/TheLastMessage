#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "BossCorePuzzleComponent.generated.h"

class ABossActivationCore;
class ABossCircularMissile;
class APlayerBase;

UCLASS()
class THELASTMESSAGE_API UBossCorePuzzleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    bool CanStartCorePuzzle(
        const TArray<TObjectPtr<ABossActivationCore>>& Cores) const;
    bool StartCorePuzzle(
        const TArray<TObjectPtr<ABossActivationCore>>& Cores);

    void ResetCorePuzzle();
    void EndCorePuzzle();
    void HandleCoreDeactivated(ABossActivationCore* Core);

    bool CanAcceptHit(const ABossActivationCore* Core) const;
    bool CheckPuzzleCompletion() const;
    int32 GetRemainingCoreCount() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StopCorePuzzle(bool bImmediate);

    void SpawnNextPuzzleMissile();
    void ScheduleNextPuzzleMissile();
    void StopPuzzleMissiles();

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CorePuzzle|Missile")
    TSubclassOf<ABossCircularMissile> PuzzleMissileClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CorePuzzle|Missile",
        meta = (ClampMin = "0.01"))
    float PuzzleMissileLaunchTime = 2.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CorePuzzle|Missile",
        meta = (ClampMin = "0.0"))
    float PuzzleMissileDamage = 10.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CorePuzzle|Missile",
        meta = (ClampMin = "1.0"))
    float PuzzleMissileDamageRadius = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|CorePuzzle|Missile")
    FVector PuzzleMissileSpawnOffset = FVector(0.f, 0.f, 150.f);

    FTimerHandle PuzzleMissileTimer;
    TArray<TWeakObjectPtr<ABossCircularMissile>> PuzzleMissiles;
    TWeakObjectPtr<APlayerBase> PuzzleMissileTarget;

    FVector FirstMissileTarget = FVector::ZeroVector;
    FVector SecondMissileTarget = FVector::ZeroVector;
    float InitialPuzzleTime = 0.f;
    int32 PuzzleMissileStep = 0;
    uint32 PuzzleMissileGeneration = 0;

    UPROPERTY()
    TArray<TObjectPtr<ABossActivationCore>> PuzzleCores;

    UPROPERTY()
    TSet<TObjectPtr<ABossActivationCore>> BrokenCores;

    bool bIsPuzzleActive = false;
    int32 RemainingCoreCount = 0;
};