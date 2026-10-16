#include "Character/Boss/BTTask_BossCircularMissile.h"
#include "Character/Boss/GA_BossCircularMissile.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_BossCircularMissile::UBTTask_BossCircularMissile()
{
    NodeName = TEXT("Boss Circular Missile");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    CircularMissileAbilityClass = UGA_BossCircularMissile::StaticClass();
}

EBTNodeResult::Type UBTTask_BossCircularMissile::ExecuteTask(
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
        || !IsValid(ASC) || !CircularMissileAbilityClass)
    {
        return EBTNodeResult::Failed;
    }

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(CircularMissileAbilityClass);
    if (!Spec || Spec->IsActive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss Circular Missile Task: ability not granted or already active. Check DefaultAbilities."));
        return EBTNodeResult::Failed;
    }

    ActiveASC = ASC;
    ActiveHandle = Spec->Handle;
    EndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(
        this, &UBTTask_BossCircularMissile::HandleAbilityEnded);

    if (!ASC->TryActivateAbility(ActiveHandle, false))
    {
        Cleanup(false);
        return EBTNodeResult::Failed;
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("SelectedAttack"),
            static_cast<uint8>(EBossAttackType::CircularMissile));
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BossCircularMissile::HandleAbilityEnded(const FAbilityEndedData& Data)
{
    if (Data.AbilitySpecHandle == ActiveHandle)
    {
        // Activation may end synchronously. Complete the BT Task on its tick.
        bAbilityEnded = true;
        bAbilityCancelled = Data.bWasCancelled;
    }
}

void UBTTask_BossCircularMissile::TickTask(
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

EBTNodeResult::Type UBTTask_BossCircularMissile::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    return EBTNodeResult::Aborted;
}

void UBTTask_BossCircularMissile::OnTaskFinished(
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

void UBTTask_BossCircularMissile::Cleanup(bool bCancelAbility)
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