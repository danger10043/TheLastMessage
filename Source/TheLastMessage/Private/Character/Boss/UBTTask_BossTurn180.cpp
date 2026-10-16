#include "Character/Boss/UBTTask_BossTurn180.h"
#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_BossTurn180::UBTTask_BossTurn180()
{
    NodeName = TEXT("Boss Turn 180");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_BossTurn180::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    ABossMonster* Boss = IsValid(AI) ? Cast<ABossMonster>(AI->GetPawn()) : nullptr;
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    UAnimInstance* Anim = IsValid(Boss) && Boss->GetMesh()
        ? Boss->GetMesh()->GetAnimInstance() : nullptr;
    UCharacterMovementComponent* Movement = IsValid(Boss)
        ? Boss->GetCharacterMovement() : nullptr;

    if (!IsValid(Boss) || !Boss->HasAuthority() || !IsValid(BB)
        || !IsValid(Anim) || !IsValid(Movement)
        || !IsValid(TurnMontage.Get()) || !TurnMontage->HasRootMotion()
        || !FMath::IsFinite(PlayRate) || PlayRate <= 0.f
        || (Boss->GetBossState() != EBossState::Idle
            && Boss->GetBossState() != EBossState::Combat))
    {
        return EBTNodeResult::Failed;
    }

    CachedBoss = Boss;
    CachedAI = AI;
    CachedAnim = Anim;
    CachedBB = BB;
    bFinished = false;
    bWasInterrupted = false;
    ActiveMontageID = INDEX_NONE;

    bSavedUseControllerYaw = Boss->bUseControllerRotationYaw;
    bSavedOrientToMovement = Movement->bOrientRotationToMovement;
    bSavedDesiredRotation = Movement->bUseControllerDesiredRotation;
    bSavedRotation = true;

    AI->StopMovement();
    AI->ClearFocus(EAIFocusPriority::Gameplay);
    Movement->StopMovementImmediately();
    Boss->bUseControllerRotationYaw = false;
    Movement->bOrientRotationToMovement = false;
    Movement->bUseControllerDesiredRotation = false;

    Boss->OnBossStateChanged.AddUniqueDynamic(
        this, &UBTTask_BossTurn180::HandleBossStateChanged);
    BB->SetValueAsBool(TEXT("bIsTurning"), true);
    BB->SetValueAsBool(TEXT("bNeedsTurn180"), false);

    if (Anim->Montage_Play(TurnMontage.Get(), PlayRate) <= 0.f)
    {
        Cleanup(true);
        return EBTNodeResult::Failed;
    }

    FAnimMontageInstance* Instance =
        Anim->GetActiveInstanceForMontage(TurnMontage.Get());
    if (!Instance)
    {
        Anim->Montage_Stop(0.f, TurnMontage.Get());
        Cleanup(false);
        return EBTNodeResult::Failed;
    }
    ActiveMontageID = Instance->GetInstanceID();

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UBTTask_BossTurn180::HandleMontageEnded);
    Anim->Montage_SetEndDelegate(EndDelegate, TurnMontage.Get());
    return EBTNodeResult::InProgress;
}

void UBTTask_BossTurn180::HandleMontageEnded(
    UAnimMontage* Montage, bool bInterrupted)
{
    bFinished = true;
    bWasInterrupted = bInterrupted;
}

void UBTTask_BossTurn180::HandleBossStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Idle && NewState != EBossState::Combat)
    {
        bFinished = true;
        bWasInterrupted = true;
        // Brain이 일시 정지되어도 몽타주와 회전 잠금은 즉시 정리한다.
        Cleanup(true);
    }
}

void UBTTask_BossTurn180::TickTask(UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
    if (bFinished)
    {
        const EBTNodeResult::Type Result = bWasInterrupted
            ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
        Cleanup(false);
        FinishLatentTask(OwnerComp, Result);
        return;
    }

    ABossMonster* Boss = CachedBoss.Get();
    UAnimInstance* Anim = CachedAnim.Get();
    AAIController* AI = CachedAI.Get();
    if (!IsValid(Boss) || !IsValid(AI) || AI->GetPawn() != Boss
        || !IsValid(Anim) || !Anim->GetMontageInstanceForID(ActiveMontageID))
    {
        Cleanup(true);
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
    }
}

EBTNodeResult::Type UBTTask_BossTurn180::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup(true);
    return EBTNodeResult::Aborted;
}

void UBTTask_BossTurn180::OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    Cleanup(true);
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_BossTurn180::Cleanup(bool bStopMontage)
{
    if (UAnimInstance* Anim = CachedAnim.Get())
    {
        if (FAnimMontageInstance* Instance =
            Anim->GetMontageInstanceForID(ActiveMontageID))
        {
            Instance->OnMontageEnded.Unbind();
            if (bStopMontage)
            {
                Anim->Montage_Stop(0.f, TurnMontage.Get());
            }
        }
    }
    ActiveMontageID = INDEX_NONE;

    if (ABossMonster* Boss = CachedBoss.Get())
    {
        Boss->OnBossStateChanged.RemoveDynamic(
            this, &UBTTask_BossTurn180::HandleBossStateChanged);
        if (bSavedRotation)
        {
            if (AAIController* AI = CachedAI.Get())
            {
                if (AI->GetPawn() == Boss)
                {
                    AI->SetControlRotation(Boss->GetActorRotation());
                }
            }
            Boss->bUseControllerRotationYaw = bSavedUseControllerYaw;
            if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
            {
                Movement->bOrientRotationToMovement = bSavedOrientToMovement;
                Movement->bUseControllerDesiredRotation = bSavedDesiredRotation;
            }
        }
    }
    bSavedRotation = false;
    if (UBlackboardComponent* BB = CachedBB.Get())
    {
        BB->SetValueAsBool(TEXT("bIsTurning"), false);
        BB->SetValueAsBool(TEXT("bNeedsTurn180"), false);
    }
    CachedBoss.Reset();
    CachedAI.Reset();
    CachedAnim.Reset();
    CachedBB.Reset();
}