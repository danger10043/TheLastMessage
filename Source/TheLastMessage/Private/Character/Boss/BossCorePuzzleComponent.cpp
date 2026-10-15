#include "Character/Boss/BossCorePuzzleComponent.h"
#include "Character/Boss/BossActivationCore.h"

void UBossCorePuzzleComponent::ResetCorePuzzle()
{
    // Destroy 콜백에서 이전 퍼즐을 다시 처리하지 않도록 먼저 비운다.
    bIsPuzzleActive = false;
    RemainingCoreCount = 0;
    TArray<TObjectPtr<ABossActivationCore>> CoresToDestroy = MoveTemp(ActiveCores);
    ActiveCores.Reset();

    for (const TObjectPtr<ABossActivationCore>& Core : CoresToDestroy)
    {
        if (!IsValid(Core.Get()))
        {
            UE_LOG(LogTemp, Verbose, TEXT("BossCorePuzzleComponent::ResetCorePuzzle - 이미 제거된 코어를 생략합니다."));
            continue;
        }
        Core->Destroy();
    }
}

void UBossCorePuzzleComponent::EndCorePuzzle()
{
    ResetCorePuzzle();
}