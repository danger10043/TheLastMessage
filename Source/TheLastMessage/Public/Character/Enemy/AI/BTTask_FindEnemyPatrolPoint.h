#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_FindEnemyPatrolPoint.generated.h"

/** HomeLocation 기준 PatrolRadius 안의 무작위 이동 가능 지점을 Blackboard에 저장 */
UCLASS()
class THELASTMESSAGE_API UBTTask_FindEnemyPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindEnemyPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolLocationKey;
};
