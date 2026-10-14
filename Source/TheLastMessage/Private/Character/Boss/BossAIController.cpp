#include "Character/Boss/BossAIController.h"

#include "Character/Boss/BossMonster.h"

void ABossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ControlledBoss = Cast<ABossMonster>(GetPawn());

    if (!IsValid(ControlledBoss.Get()))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossAIController::OnPossess [%s] - Controlled pawn is not a valid BossMonster: %s"),
            *GetName(), *GetNameSafe(GetPawn()));
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("BossAIController::OnPossess [%s] - Connected to boss: %s"),
        *GetName(), *GetNameSafe(ControlledBoss.Get()));

    // Blackboard와 Behavior Tree 구성 후 StartBossAI()를 호출한다.
}