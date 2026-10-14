#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ClearEnemyBlackboardValue.generated.h"

/** 선택한 Blackboard 키 값을 비운다 (예: 조사를 마친 뒤 LastKnownLocation 해제) */
UCLASS()
class THELASTMESSAGE_API UBTTask_ClearEnemyBlackboardValue : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_ClearEnemyBlackboardValue();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
