#include "Character/Boss/BTTask_BossCrossLaser.h"
#include "Character/Boss/GA_BossCrossLaser.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_BossCrossLaser::UBTTask_BossCrossLaser()
{
    NodeName = TEXT("Boss Cross Laser");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    CrossLaserAbilityClass = UGA_BossCrossLaser::StaticClass();
}

EBTNodeResult::Type UBTTask_BossCrossLaser::ExecuteTask(
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
        || !IsValid(ASC) || !CrossLaserAbilityClass)
    {
        const TCHAR* FailureReason = !IsValid(AI)
            ? TEXT("AI controller is invalid")
            : !IsValid(Boss)
            ? TEXT("AI pawn is missing or is not an ABossMonster")
            : !Boss->HasAuthority()
            ? TEXT("boss has no server authority")
            : !IsValid(ASC)
            ? TEXT("boss ASC is invalid")
            : TEXT("CrossLaserAbilityClass is unset");

        UE_LOG(LogTemp, Warning,
            TEXT("Boss Cross Laser Task: ExecuteTask failed: %s. AI=%s Boss=%s"),
            FailureReason, *GetNameSafe(AI), *GetNameSafe(Boss));
        return EBTNodeResult::Failed;
    }

    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(CrossLaserAbilityClass);
    if (!Spec || Spec->IsActive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss Cross Laser Task: ExecuteTask failed: %s. Boss=%s AbilityClass=%s"),
            !Spec ? TEXT("no granted spec matches the exact ability class")
            : TEXT("ability is already active"),
            *GetNameSafe(Boss), *GetNameSafe(CrossLaserAbilityClass.Get()));
        return EBTNodeResult::Failed;
    }

    ActiveASC = ASC;
    ActiveHandle = Spec->Handle;
    EndedDelegateHandle = ASC->OnAbilityEnded.AddUObject(
        this, &UBTTask_BossCrossLaser::HandleAbilityEnded);

    FGameplayTagContainer FailureTags;
    const FDelegateHandle FailureDelegateHandle = ASC->AbilityFailedCallbacks.AddLambda(
        [AbilityClass = CrossLaserAbilityClass.Get(), &FailureTags](
            const UGameplayAbility* FailedAbility, const FGameplayTagContainer& Tags)
        {
            if (FailedAbility && FailedAbility->GetClass() == AbilityClass)
            {
                FailureTags.AppendTags(Tags);
            }
        });

    const bool bActivated = ASC->TryActivateAbility(ActiveHandle, false);
    ASC->AbilityFailedCallbacks.Remove(FailureDelegateHandle);

    if (!bActivated)
    {
        const FString FailureTagText = FailureTags.IsEmpty()
            ? TEXT("<none>") : FailureTags.ToString();
        UE_LOG(LogTemp, Warning,
            TEXT("Boss Cross Laser Task: TryActivateAbility returned false. Boss=%s AbilityClass=%s FailureTags=%s"),
            *GetNameSafe(Boss), *GetNameSafe(CrossLaserAbilityClass.Get()), *FailureTagText);
        Cleanup(false);
        return EBTNodeResult::Failed;
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("SelectedAttack"),
            static_cast<uint8>(EBossAttackType::CrossLaser));
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BossCrossLaser::HandleAbilityEnded(const FAbilityEndedData& Data)
{
    if (Data.AbilitySpecHandle == ActiveHandle)
    {
        // Activation may end synchronously. Complete the BT Task on its tick.
        bAbilityEnded = true;
        bAbilityCancelled = Data.bWasCancelled;
    }
}

void UBTTask_BossCrossLaser::TickTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    if (bAbilityEnded)
    {
        if (bAbilityCancelled)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Boss Cross Laser Task: ability ended with bWasCancelled=true. AI=%s AbilityClass=%s"),
                *GetNameSafe(OwnerComp.GetAIOwner()), *GetNameSafe(CrossLaserAbilityClass.Get()));
        }
        FinishLatentTask(OwnerComp,
            bAbilityCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
        return;
    }

    UAbilitySystemComponent* ASC = ActiveASC.Get();
    const FGameplayAbilitySpec* Spec = IsValid(ASC)
        ? ASC->FindAbilitySpecFromHandle(ActiveHandle) : nullptr;
    if (!Spec || !Spec->IsActive())
    {
        const TCHAR* FailureReason = !IsValid(ASC)
            ? TEXT("tracked ASC is invalid")
            : !Spec ? TEXT("tracked ability spec is missing")
            : TEXT("tracked ability spec is inactive");

        UE_LOG(LogTemp, Warning,
            TEXT("Boss Cross Laser Task: no matching ability-ended callback: %s. AI=%s AbilityClass=%s"),
            FailureReason, *GetNameSafe(OwnerComp.GetAIOwner()),
            *GetNameSafe(CrossLaserAbilityClass.Get()));
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
    }
}

EBTNodeResult::Type UBTTask_BossCrossLaser::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    return EBTNodeResult::Aborted;
}

void UBTTask_BossCrossLaser::OnTaskFinished(
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

void UBTTask_BossCrossLaser::Cleanup(bool bCancelAbility)
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