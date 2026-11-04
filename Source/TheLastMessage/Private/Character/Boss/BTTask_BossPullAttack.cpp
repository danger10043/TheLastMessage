#include "Character/Boss/BTTask_BossPullAttack.h"
#include "Character/Boss/GA_BossPullAttack.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_BossPullAttack::UBTTask_BossPullAttack()
{
    NodeName = TEXT("Boss Pull Attack");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    PullAbilityClass = UGA_BossPullAttack::StaticClass();
}

EBTNodeResult::Type UBTTask_BossPullAttack::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    bAbilityEnded = false;
    bAbilityCancelled = false;

    AAIController* AI = OwnerComp.GetAIOwner();
    ABossMonster* Boss = IsValid(AI) ? Cast<ABossMonster>(AI->GetPawn()) : nullptr;
    UAbilitySystemComponent* ASC = IsValid(Boss)
        ? Boss->GetAbilitySystemComponent() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !IsValid(ASC) || !PullAbilityClass)
    {
        return EBTNodeResult::Failed;
    }

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(PullAbilityClass);
    if (!Spec || Spec->IsActive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss Pull Attack Task: ability not granted or already active. Check DefaultAbilities."));
        return EBTNodeResult::Failed;
    }

    ActiveASC = ASC;
    ActiveHandle = Spec->Handle;
    EndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(
        this, &UBTTask_BossPullAttack::HandleAbilityEnded);

    if (!ASC->TryActivateAbility(ActiveHandle, false))
    {
        Cleanup(false);
        return EBTNodeResult::Failed;
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("SelectedAttack"),
            static_cast<uint8>(EBossAttackType::PullAttack));
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BossPullAttack::HandleAbilityEnded(const FAbilityEndedData& Data)
{
    if (Data.AbilitySpecHandle == ActiveHandle)
    {
        // Activation may end synchronously. Complete the BT Task on its tick.
        bAbilityEnded = true;
        bAbilityCancelled = Data.bWasCancelled;
    }
}

void UBTTask_BossPullAttack::TickTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    if (bAbilityEnded)
    {
        FinishLatentTask(OwnerComp,
            bAbilityCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
        return;
    }

    UAbilitySystemComponent* ASC = ActiveASC.Get();
    const FGameplayAbilitySpec* Spec = IsValid(ASC)
        ? ASC->FindAbilitySpecFromHandle(ActiveHandle) : nullptr;
    if (!Spec || !Spec->IsActive())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
    }
}

EBTNodeResult::Type UBTTask_BossPullAttack::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    return EBTNodeResult::Aborted;
}

void UBTTask_BossPullAttack::OnTaskFinished(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    Cleanup(true);
    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("SelectedAttack"),
            static_cast<uint8>(EBossAttackType::None));
    }
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_BossPullAttack::Cleanup(bool bCancelAbility)
{
    if (UAbilitySystemComponent* ASC = ActiveASC.Get())
    {
        ASC->OnAbilityEnded.Remove(EndedDelegateHandle);
        if (bCancelAbility && ActiveHandle.IsValid())
        {
            ASC->CancelAbilityHandle(ActiveHandle);
        }
    }
    EndedDelegateHandle.Reset();
    ActiveASC.Reset();
    ActiveHandle = FGameplayAbilitySpecHandle();
}