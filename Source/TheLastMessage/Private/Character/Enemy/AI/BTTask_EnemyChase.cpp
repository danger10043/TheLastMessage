#include "Character/Enemy/AI/BTTask_EnemyChase.h"
#include "Character/Enemy/EnemyBase.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_EnemyChase::UBTTask_EnemyChase()
{
	NodeName = TEXT("Enemy Chase (Move To Target)");

	// 몬스터마다 허용 반경이 다르므로, 같은 BT를 쓰더라도 노드를 몬스터별로 따로 만든다.
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_EnemyChase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIC = OwnerComp.GetAIOwner();
	const AEnemyBase* Enemy = AIC ? Cast<AEnemyBase>(AIC->GetPawn()) : nullptr;
	if (Enemy)
	{
		const FEnemyStatConfig& Stats = Enemy->GetStatConfig();
		AcceptableRadius = Stats.ChaseAcceptableRadius > 0.f ? Stats.ChaseAcceptableRadius : Stats.AttackRange * 0.8f;
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
