#include "Character/Enemy/EnemyAIController.h"
#include "Character/Enemy/EnemyBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AEnemyAIController::AEnemyAIController()
{
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent")));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->PeripheralVisionAngleDegrees = 180.f;

	GetAIPerceptionComponent()->ConfigureSense(*SightConfig);
	GetAIPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledEnemy = Cast<AEnemyBase>(InPawn);
	if (!ControlledEnemy)
	{
		return;
	}

	// 컨트롤러가 폰의 BeginPlay보다 먼저 빙의될 수 있으므로 여기서도 초기화한다(중복 호출 안전).
	ControlledEnemy->InitializeEnemy();

	const float DetectRange = ControlledEnemy->GetStatConfig().DetectRange;
	SightConfig->SightRadius = DetectRange;
	SightConfig->LoseSightRadius = DetectRange + 200.f;
	GetAIPerceptionComponent()->ConfigureSense(*SightConfig);

	GetAIPerceptionComponent()->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);
	ControlledEnemy->OnEnemyStateChanged.AddUniqueDynamic(this, &AEnemyAIController::UpdateEnemyState);

	StartEnemyAI();
}

void AEnemyAIController::OnUnPossess()
{
	if (ControlledEnemy)
	{
		ControlledEnemy->OnEnemyStateChanged.RemoveDynamic(this, &AEnemyAIController::UpdateEnemyState);
	}
	GetAIPerceptionComponent()->OnTargetPerceptionUpdated.RemoveDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);

	GetWorldTimerManager().ClearTimer(BlackboardUpdateTimer);
	ControlledEnemy = nullptr;

	Super::OnUnPossess();
}

void AEnemyAIController::StartEnemyAI()
{
	if (!ControlledEnemy)
	{
		return;
	}

	if (BlackboardAsset)
	{
		UBlackboardComponent* BBComp = nullptr;
		UseBlackboard(BlackboardAsset, BBComp);
	}

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsVector(EnemyBB::HomeLocation, ControlledEnemy->GetHomeLocation());
		BB->SetValueAsEnum(EnemyBB::EnemyState, static_cast<uint8>(ControlledEnemy->GetEnemyState()));
		BB->SetValueAsBool(EnemyBB::bCanAttack, false);
	}

	GetWorldTimerManager().SetTimer(BlackboardUpdateTimer, this, &AEnemyAIController::UpdateBlackboardTargetInfo, 0.1f, true);
}

void AEnemyAIController::RestartEnemyAI()
{
	StopEnemyAI();

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(EnemyBB::TargetActor);
		BB->ClearValue(EnemyBB::LastKnownLocation);
		BB->ClearValue(EnemyBB::PatrolLocation);
	}

	// 이전에 감지했던 대상을 잊어야 다시 발견 이벤트가 발생한다.
	if (UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		Perception->ForgetAll();
	}

	StartEnemyAI();
}

void AEnemyAIController::StopEnemyAI()
{
	GetWorldTimerManager().ClearTimer(BlackboardUpdateTimer);
	StopMovement();

	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("EnemyDead"));
	}
}

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!ControlledEnemy || ControlledEnemy->IsDead() || ControlledEnemy->IsPooled() || !Actor)
	{
		return;
	}

	// 플레이어가 조종하는 폰만 대상으로 삼는다.
	const APawn* SensedPawn = Cast<APawn>(Actor);
	if (!SensedPawn || !SensedPawn->IsPlayerControlled())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		ControlledEnemy->OnPlayerDetected(Actor);
	}
	else if (ControlledEnemy->GetCurrentTarget() == Actor)
	{
		// 놓친 위치를 기억해 둔다. BT의 조사 분기가 그 위치로 이동해 잠시 대기한다.
		if (UBlackboardComponent* BB = GetBlackboardComponent())
		{
			BB->SetValueAsVector(EnemyBB::LastKnownLocation, Actor->GetActorLocation());
		}
		ControlledEnemy->OnPlayerLost();
	}
}

void AEnemyAIController::UpdateTargetActor(AActor* NewTarget)
{
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		if (NewTarget)
		{
			BB->SetValueAsObject(EnemyBB::TargetActor, NewTarget);
			BB->ClearValue(EnemyBB::LastKnownLocation); // 다시 발견하면 조사 중단
		}
		else
		{
			BB->ClearValue(EnemyBB::TargetActor);
			BB->SetValueAsBool(EnemyBB::bCanAttack, false);
		}
	}
}

void AEnemyAIController::UpdateEnemyState(EEnemyState NewState)
{
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsEnum(EnemyBB::EnemyState, static_cast<uint8>(NewState));
	}
}

void AEnemyAIController::UpdateBlackboardTargetInfo()
{
	if (!ControlledEnemy || ControlledEnemy->IsDead() || !ControlledEnemy->GetCurrentTarget())
	{
		return;
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	const float Distance = ControlledEnemy->GetDistanceToTarget();
	BB->SetValueAsFloat(EnemyBB::TargetDistance, Distance);
	BB->SetValueAsBool(EnemyBB::bCanAttack, ControlledEnemy->CanAttackTarget());

	// 공격 범위 안이면 Attack, 밖이면 Chase
	const bool bInAttackRange = Distance <= ControlledEnemy->GetStatConfig().AttackRange;
	ControlledEnemy->SetEnemyState(bInAttackRange ? EEnemyState::Attack : EEnemyState::Chase);
}
