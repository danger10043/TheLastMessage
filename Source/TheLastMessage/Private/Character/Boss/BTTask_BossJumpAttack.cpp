#include "Character/Boss/BTTask_BossJumpAttack.h"
#include "Character/Boss/GA_BossJumpAttack.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_BossJumpAttack::UBTTask_BossJumpAttack()
{
    NodeName = TEXT("Boss Jump Attack");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    JumpAbilityClass = UGA_BossJumpAttack::StaticClass();
}

EBTNodeResult::Type UBTTask_BossJumpAttack::ExecuteTask(
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
        || !IsValid(ASC) || !JumpAbilityClass)
    {
        return EBTNodeResult::Failed;
    }

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(JumpAbilityClass);
    if (!Spec || Spec->IsActive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss Jump Attack Task: ability not granted or already active. Check DefaultAbilities."));
        return EBTNodeResult::Failed;
    }

    ActiveASC = ASC;
    ActiveHandle = Spec->Handle;
    EndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(
        this, &UBTTask_BossJumpAttack::HandleAbilityEnded);

    if (!ASC->TryActivateAbility(ActiveHandle, false))
    {
        Cleanup(false);
        return EBTNodeResult::Failed;
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("SelectedAttack"),
            static_cast<uint8>(EBossAttackType::JumpAttack));
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BossJumpAttack::HandleAbilityEnded(const FAbilityEndedData& Data)
{
    if (Data.AbilitySpecHandle == ActiveHandle)
    {
        // Activation may end synchronously. Complete the BT Task on its tick.
        bAbilityEnded = true;
        bAbilityCancelled = Data.bWasCancelled;
    }
}

void UBTTask_BossJumpAttack::TickTask(
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

EBTNodeResult::Type UBTTask_BossJumpAttack::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    return EBTNodeResult::Aborted;
}

void UBTTask_BossJumpAttack::OnTaskFinished(
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

void UBTTask_BossJumpAttack::Cleanup(bool bCancelAbility)
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