#include "Character/Boss/BTTaskNode_BossGunBurst.h"
#include "Character/Boss/GA_BossGunBurst.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_BossGunBurst::UBTTask_BossGunBurst()
{
    NodeName = TEXT("Boss Gun Burst");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    GunBurstAbilityClass = UGA_BossGunBurst::StaticClass();
}

EBTNodeResult::Type UBTTask_BossGunBurst::ExecuteTask(
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
        || !IsValid(ASC) || !GunBurstAbilityClass)
    {
        return EBTNodeResult::Failed;
    }

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(GunBurstAbilityClass);
    if (!Spec || Spec->IsActive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss Gun Burst Task: ability not granted or already active. Check DefaultAbilities."));
        return EBTNodeResult::Failed;
    }

    ActiveASC = ASC;
    ActiveHandle = Spec->Handle;
    EndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(
        this, &UBTTask_BossGunBurst::HandleAbilityEnded);

    if (!ASC->TryActivateAbility(ActiveHandle, false))
    {
        Cleanup(false);
        return EBTNodeResult::Failed;
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("SelectedAttack"),
            static_cast<uint8>(EBossAttackType::GunBurst));
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BossGunBurst::HandleAbilityEnded(const FAbilityEndedData& Data)
{
    if (Data.AbilitySpecHandle == ActiveHandle)
    {
        // Activation may end synchronously. Complete the BT Task on its tick.
        bAbilityEnded = true;
        bAbilityCancelled = Data.bWasCancelled;
    }
}

void UBTTask_BossGunBurst::TickTask(
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

EBTNodeResult::Type UBTTask_BossGunBurst::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    return EBTNodeResult::Aborted;
}

void UBTTask_BossGunBurst::OnTaskFinished(
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

void UBTTask_BossGunBurst::Cleanup(bool bCancelAbility)
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