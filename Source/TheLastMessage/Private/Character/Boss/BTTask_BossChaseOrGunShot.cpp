#include "Character/Boss/BTTask_BossChaseOrGunShot.h"
#include "Character/Boss/GA_BossGunShot.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_BossChaseOrGunShot::UBTTask_BossChaseOrGunShot()
{
    NodeName = TEXT("Boss Chase Or Gun Shot");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    GunShotAbilityClass = UGA_BossGunShot::StaticClass();
}

EBTNodeResult::Type UBTTask_BossChaseOrGunShot::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup();
    bMoveFinished = false;
    bMoveFailed = false;
    bMoveSucceeded = false;
    bAbilityEnded = false;
    bAbilityCancelled = false;
    AAIController* AI = OwnerComp.GetAIOwner();
    ABossMonster* Boss = IsValid(AI) ? Cast<ABossMonster>(AI->GetPawn()) : nullptr;
    APlayerBase* Player = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;
    UPathFollowingComponent* Following = IsValid(AI) ? AI->GetPathFollowingComponent() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !IsValid(Following)
        || !IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead()
        || (Boss->GetBossState() != EBossState::Idle
            && Boss->GetBossState() != EBossState::Combat)
        || !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0.f)
        return EBTNodeResult::Failed;

    Target = Player;
    LockedPhase = Boss->GetBossPhase();
    PathFollowing = Following;
    MoveDelegate = Following->OnRequestFinished.AddUObject(
        this, &UBTTask_BossChaseOrGunShot::HandleMoveFinished);
    bMoving = true;

    FAIMoveRequest Request(Player);
    Request.SetUsePathfinding(true);
    Request.SetAllowPartialPath(false);
    Request.SetRequireNavigableEndLocation(true);
    Request.SetProjectGoalLocation(false);
    Request.SetAcceptanceRadius(AcceptanceRadius);
    Request.SetReachTestIncludesAgentRadius(false);
    Request.SetReachTestIncludesGoalRadius(false);
    const FPathFollowingRequestResult Result = AI->MoveTo(Request);
    MoveRequestID = Result.MoveId;
    if (Result.Code != EPathFollowingRequestResult::RequestSuccessful)
    {
        bMoveFinished = true;
        bMoveFailed = Result.Code == EPathFollowingRequestResult::Failed;
        bMoveSucceeded = Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
    }
    return EBTNodeResult::InProgress;
}

void UBTTask_BossChaseOrGunShot::HandleMoveFinished(
    FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    if (!bMoving || RequestID != MoveRequestID) return;
    bMoveFinished = true;
    bMoveSucceeded = Result.Code == EPathFollowingResult::Success;
    bMoveFailed = Result.Code == EPathFollowingResult::Blocked
        || Result.Code == EPathFollowingResult::OffPath
        || Result.Code == EPathFollowingResult::Invalid
        || (Result.Code == EPathFollowingResult::Aborted
            && Result.HasFlag(FPathFollowingResultFlags::InvalidPath)
            && !Result.HasFlag(FPathFollowingResultFlags::NewRequest
                | FPathFollowingResultFlags::OwnerFinished));
    // A cancelled chase is excluded; an invalidated navigation path is a failure.
}

void UBTTask_BossChaseOrGunShot::TickTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    ABossMonster* Boss = IsValid(AI) ? Cast<ABossMonster>(AI->GetPawn()) : nullptr;
    APlayerBase* Player = Target.Get();
    if (!IsValid(Boss) || !Boss->HasAuthority() || !IsValid(Player)
        || Player->IsDead() || !Player->IsPlayerControlled()
        || Boss->GetCurrentTarget() != Player || Boss->GetBossPhase() != LockedPhase
        || Boss->GetBossState() == EBossState::Dead
        || Boss->GetBossState() == EBossState::ShieldPuzzle
        || Boss->GetBossState() == EBossState::Cinematic)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    if (bMoving)
    {
        if (!bMoveFinished)
        {
            if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
                BB && BB->GetValueAsBool(TEXT("bIsInAttackRange")))
                FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
            return;
        }
        bMoving = false;
        if (UPathFollowingComponent* Following = PathFollowing.Get())
            Following->OnRequestFinished.Remove(MoveDelegate);
        MoveDelegate.Reset();
        if (!bMoveFailed)
        {
            FinishLatentTask(OwnerComp,
                bMoveSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
            return;
        }
        if ((LockedPhase != EBossPhase::Phase1 && LockedPhase != EBossPhase::Phase2)
            || !GunShotAbilityClass)
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
        UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
        FGameplayAbilitySpec* Spec = IsValid(ASC)
            ? ASC->FindAbilitySpecFromClass(GunShotAbilityClass) : nullptr;
        if (!Spec || Spec->IsActive())
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
        ActiveASC = ASC;
        ActiveHandle = Spec->Handle;
        AbilityDelegate = ASC->OnAbilityEnded.AddUObject(
            this, &UBTTask_BossChaseOrGunShot::HandleAbilityEnded);
        if (!ASC->TryActivateAbility(ActiveHandle, false))
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }
        if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
            BB->SetValueAsEnum(TEXT("SelectedAttack"), static_cast<uint8>(EBossAttackType::GunShot));
        return;
    }
    if (bAbilityEnded)
    {
        FinishLatentTask(OwnerComp,
            bAbilityCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
        return;
    }
    UAbilitySystemComponent* ASC = ActiveASC.Get();
    const FGameplayAbilitySpec* Spec = IsValid(ASC)
        ? ASC->FindAbilitySpecFromHandle(ActiveHandle) : nullptr;
    if (!Spec || !Spec->IsActive()) FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
}

void UBTTask_BossChaseOrGunShot::HandleAbilityEnded(const FAbilityEndedData& Data)
{
    if (Data.AbilitySpecHandle == ActiveHandle)
    {
        bAbilityEnded = true;
        bAbilityCancelled = Data.bWasCancelled;
    }
}

EBTNodeResult::Type UBTTask_BossChaseOrGunShot::AbortTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup();
    return EBTNodeResult::Aborted;
}

void UBTTask_BossChaseOrGunShot::OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    Cleanup();
    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
        BB->SetValueAsEnum(TEXT("SelectedAttack"), static_cast<uint8>(EBossAttackType::None));
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_BossChaseOrGunShot::Cleanup()
{
    if (UPathFollowingComponent* Following = PathFollowing.Get())
    {
        Following->OnRequestFinished.Remove(MoveDelegate);
        if (bMoving && MoveRequestID.IsValid()
            && Following->GetCurrentRequestId() == MoveRequestID)
            Following->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
    }
    if (UAbilitySystemComponent* ASC = ActiveASC.Get())
    {
        ASC->OnAbilityEnded.Remove(AbilityDelegate);
        if (ActiveHandle.IsValid()) ASC->CancelAbilityHandle(ActiveHandle);
    }
    MoveDelegate.Reset();
    AbilityDelegate.Reset();
    MoveRequestID = FAIRequestID::InvalidRequest;
    ActiveHandle = FGameplayAbilitySpecHandle();
    PathFollowing.Reset();
    ActiveASC.Reset();
    Target.Reset();
    bMoving = false;
}