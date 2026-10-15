#include "Character/Enemy/AI/BTTask_ClearEnemyBlackboardValue.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearEnemyBlackboardValue::UBTTask_ClearEnemyBlackboardValue()
{
	NodeName = TEXT("Clear Enemy Blackboard Value");
}

EBTNodeResult::Type UBTTask_ClearEnemyBlackboardValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	BB->ClearValue(GetSelectedBlackboardKey());
	return EBTNodeResult::Succeeded;
}
