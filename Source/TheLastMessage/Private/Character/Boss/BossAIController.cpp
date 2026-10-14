#include "Character/Boss/BossAIController.h"

#include "Character/Boss/BossMonster.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

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

    StartBossAI();
}

void ABossAIController::StartBossAI()
{
    if (!HasAuthority() || !IsValid(ControlledBoss.Get())
        || ControlledBoss.Get() != GetPawn()
        || ControlledBoss->GetBossState() == EBossState::Dead)
    {
        return;
    }

    if (!IsValid(BehaviorTreeAsset.Get()) || !IsValid(BlackboardAsset.Get()))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossAIController::StartBossAI - Configure BehaviorTreeAsset and BlackboardAsset."));
        return;
    }

    if (BehaviorTreeAsset->BlackboardAsset != BlackboardAsset.Get())
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossAIController::StartBossAI - Behavior Tree Blackboard does not match BlackboardAsset."));
        return;
    }

    UBlackboardComponent* BlackboardComponent = nullptr;
    if (!UseBlackboard(BlackboardAsset.Get(), BlackboardComponent)
        || !IsValid(BlackboardComponent))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossAIController::StartBossAI - Failed to initialize Blackboard."));
        return;
    }

    BlackboardComponent->ClearValue(TEXT("TargetActor"));
    BlackboardComponent->SetValueAsEnum(TEXT("BossPhase"), static_cast<uint8>(ControlledBoss->GetBossPhase()));
    BlackboardComponent->SetValueAsEnum(TEXT("BossState"), static_cast<uint8>(ControlledBoss->GetBossState()));
    BlackboardComponent->SetValueAsFloat(TEXT("TargetDistance"), TNumericLimits<float>::Max());
    BlackboardComponent->SetValueAsEnum(TEXT("SelectedAttack"), static_cast<uint8>(EBossAttackType::None));
    BlackboardComponent->SetValueAsBool(TEXT("bIsShieldActive"), false);
    BlackboardComponent->SetValueAsBool(TEXT("bIsPuzzleActive"), false);
    BlackboardComponent->SetValueAsEnum(TEXT("PuzzleType"), static_cast<uint8>(EBossPuzzleType::None));

    if (!RunBehaviorTree(BehaviorTreeAsset.Get()))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossAIController::StartBossAI - Failed to start Behavior Tree."));
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("BossAIController::StartBossAI [%s] - Behavior Tree started: %s"),
        *GetName(), *GetNameSafe(BehaviorTreeAsset.Get()));
}