#include "Character/Enemy/AI/BTTask_EnemyAttack.h"
#include "Character/Enemy/EnemyBase.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_EnemyAttack::UBTTask_EnemyAttack()
{
	NodeName = TEXT("Enemy Attack");
}

EBTNodeResult::Type UBTTask_EnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIC = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = AIC ? Cast<AEnemyBase>(AIC->GetPawn()) : nullptr;
	if (!Enemy || Enemy->IsDead())
	{
		return EBTNodeResult::Failed;
	}

	Enemy->StartAttack();
	return EBTNodeResult::Succeeded;
}
