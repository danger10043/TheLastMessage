#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BTTask_EnemyChase.generated.h"

/**
 * 타겟을 쫓아가는 Move To. 허용 반경(멈추는 거리)을 BT 노드가 아닌
 * 몬스터의 데이터(StatConfig.ChaseAcceptableRadius)에서 가져온다.
 * Blackboard Key에는 TargetActor를 지정한다.
 */
UCLASS()
class THELASTMESSAGE_API UBTTask_EnemyChase : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_EnemyChase();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
