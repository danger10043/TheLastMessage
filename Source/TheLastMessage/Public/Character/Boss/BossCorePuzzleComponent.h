#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossCorePuzzleComponent.generated.h"

class ABossActivationCore;
class ATargetPoint;

UCLASS()
class THELASTMESSAGE_API UBossCorePuzzleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void StartCorePuzzle();
    void SpawnActivationCores();
    void ResetCorePuzzle();
    void EndCorePuzzle();
    void HandleCoreDeactivated(ABossActivationCore* Core);
    bool CheckPuzzleCompletion();
    int32 GetRemainingCoreCount();

private:
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<ABossActivationCore> CoreClass;

    // 명세에서 미정. 0은 설정 전 임시값이다.
    UPROPERTY(EditDefaultsOnly)
    int32 CoreCount = 0;

    UPROPERTY()
    TArray<TObjectPtr<ABossActivationCore>> ActiveCores;

    UPROPERTY()
    int32 RemainingCoreCount = 0;

    UPROPERTY(EditInstanceOnly)
    TArray<TObjectPtr<ATargetPoint>> CoreSpawnPoints;

    UPROPERTY()
    bool bIsPuzzleActive = false;
};