#include "Character/Enemy/AI/BTTask_FindEnemyPatrolPoint.h"
#include "Character/Enemy/EnemyBase.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "NavigationSystem.h"

UBTTask_FindEnemyPatrolPoint::UBTTask_FindEnemyPatrolPoint()
{
	NodeName = TEXT("Find Enemy Patrol Point");
	PatrolLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindEnemyPatrolPoint, PatrolLocationKey));
}

EBTNodeResult::Type UBTTask_FindEnemyPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIC = OwnerComp.GetAIOwner();
	const AEnemyBase* Enemy = AIC ? Cast<AEnemyBase>(AIC->GetPawn()) : nullptr;
	if (!Enemy)
	{
		return EBTNodeResult::Failed;
	}

	if (PatrolLocationKey.SelectedKeyName.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	// 정찰 축(PatrolAxis) 위에서만 홈 기준 좌우로 왕복한다. 너무 가까운 지점은 피한다.
	const FVector Axis = Enemy->GetPatrolAxis();
	const float Radius = Enemy->GetPatrolRadius();
	const float Sign = FMath::RandBool() ? 1.f : -1.f;
	const float Offset = Sign * FMath::FRandRange(Radius * 0.3f, Radius);
	const FVector Home = Enemy->GetHomeLocation();
	const FVector Current = Enemy->GetActorLocation();
	FVector Target = Home + Axis * Offset;

	// 수평 축이면 높이·평면 좌표는 몬스터의 현재 위치를 따른다.
	// (홈이 공중에서 기록됐거나 높이가 어긋나도 NavMesh에 투영되도록 한다.)
	if (FMath::Abs(Axis.Z) < 0.5f)
	{
		const float HomeAlongAxis = FVector::DotProduct(Home - Current, Axis);
		Target = Current + Axis * (HomeAlongAxis + Offset);
	}

	// 수평 축(지상 이동)일 때만 NavMesh에 맞춘다. 벽·천장 이동(Z축)은 NavMesh를 쓰지 않는다.
	if (FMath::Abs(Axis.Z) < 0.5f)
	{
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(OwnerComp.GetWorld());
		FNavLocation NavLocation;
		if (!NavSys)
		{
			return EBTNodeResult::Failed;
		}
		if (!NavSys->ProjectPointToNavigation(Target, NavLocation, FVector(50.f, 50.f, 200.f)))
		{
			return EBTNodeResult::Failed;
		}
		Target = NavLocation.Location;
	}

	OwnerComp.GetBlackboardComponent()->SetValueAsVector(PatrolLocationKey.SelectedKeyName, Target);
	return EBTNodeResult::Succeeded;
}
