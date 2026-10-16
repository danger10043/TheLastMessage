#include "Character/Boss/UBTService_BossCombatRange.h"
#include "Character/Boss/BossAIController.h"
#include "Character/Boss/BossMonster.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_BossCombatRange::UBTService_BossCombatRange()
{
    NodeName = TEXT("Update Boss Combat Range");
    bNotifyBecomeRelevant = true;
    bNotifyTick = true;
    Interval = 0.05f;
    RandomDeviation = 0.f;
}

void UBTService_BossCombatRange::OnBecomeRelevant(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Super::OnBecomeRelevant(OwnerComp, NodeMemory);
    UpdateValues(OwnerComp);
}

void UBTService_BossCombatRange::TickNode(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    UpdateValues(OwnerComp);
}

void UBTService_BossCombatRange::UpdateValues(UBehaviorTreeComponent& OwnerComp)
{
    ABossAIController* AI = Cast<ABossAIController>(OwnerComp.GetAIOwner());
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!IsValid(AI) || !AI->HasAuthority() || !IsValid(BB))
    {
        return;
    }

    AI->UpdateCombatRange(AttackType, CombatRadius);

    ABossMonster* Boss = Cast<ABossMonster>(AI->GetPawn());
    AActor* Target = IsValid(Boss) ? Boss->GetCurrentTarget() : nullptr;
    bool bNeedsTurn = false;
    if (IsValid(Boss) && IsValid(Target)
        && BB->GetValueAsBool(TEXT("bIsInCombatRange"))
        && !BB->GetValueAsBool(TEXT("bIsTurning"))
        && (Boss->GetBossState() == EBossState::Idle
            || Boss->GetBossState() == EBossState::Combat))
    {
        const FVector ToTarget = (Target->GetActorLocation()
            - Boss->GetActorLocation()).GetSafeNormal2D();
        const FVector Forward = Boss->GetActorForwardVector().GetSafeNormal2D();
        if (!ToTarget.IsNearlyZero() && !Forward.IsNearlyZero())
        {
            const float Threshold = FMath::Clamp(TurnThresholdDegrees, 90.f, 180.f);
            bNeedsTurn = FVector::DotProduct(Forward, ToTarget)
                <= FMath::Cos(FMath::DegreesToRadians(Threshold));
        }
    }
    BB->SetValueAsBool(TEXT("bNeedsTurn180"), bNeedsTurn);
}