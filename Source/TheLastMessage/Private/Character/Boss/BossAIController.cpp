#include "Character/Boss/BossAIController.h"

#include "Character/Boss/BossMonster.h"

void ABossAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ControlledBoss = Cast<ABossMonster>(InPawn);

    // StartBossAI()를 구현한 뒤 유효한 보스에 대해 호출한다.
}