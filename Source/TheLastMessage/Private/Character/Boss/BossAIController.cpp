#include "Character/Boss/BossAIController.h"

#include "Character/Boss/BossMonster.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "GameFramework/Pawn.h"
#include "Misc/ScopeExit.h"

ABossAIController::ABossAIController()
{
    UAIPerceptionComponent* BossPerception =
        CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("BossPerception"));
    SetPerceptionComponent(*BossPerception);

    SightConfig =
        CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 2000.f;
    SightConfig->LoseSightRadius = 2500.f;
    SightConfig->PeripheralVisionAngleDegrees = 180.f;
    SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

    BossPerception->ConfigureSense(*SightConfig);
    BossPerception->SetDominantSense(UAISense_Sight::StaticClass());
    BossPerception->OnPerceptionUpdated.AddDynamic(
        this, &ABossAIController::HandlePerceptionUpdated);
}

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
    BlackboardComponent->SetValueAsBool(TEXT("bIsInCombatRange"), false);
    BlackboardComponent->SetValueAsBool(TEXT("bIsInAttackRange"), false);
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

    RefreshTargetFromPerception();
}

void ABossAIController::HandlePerceptionUpdated(
    const TArray<AActor*>& UpdatedActors)
{
    if (UpdatedActors.Num() > 0)
    {
        RefreshTargetFromPerception();
    }
}

void ABossAIController::RefreshTargetFromPerception()
{
    UAIPerceptionComponent* BossPerception = GetPerceptionComponent();
    if (!HasAuthority() || !IsValid(ControlledBoss.Get())
        || ControlledBoss.Get() != GetPawn()
        || !IsValid(BossPerception) || !IsValid(GetBlackboardComponent()))
    {
        return;
    }

    if (ControlledBoss->GetBossState() == EBossState::Dead)
    {
        UpdateTargetActor(nullptr);
        return;
    }

    TArray<AActor*> SeenActors;
    BossPerception->GetCurrentlyPerceivedActors(
        UAISense_Sight::StaticClass(), SeenActors);

    AActor* CurrentTarget = ControlledBoss->GetCurrentTarget();
    AActor* NewTarget = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (AActor* SeenActor : SeenActors)
    {
        APawn* PlayerPawn = Cast<APawn>(SeenActor);
        if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
        {
            continue;
        }

        if (PlayerPawn == CurrentTarget)
        {
            NewTarget = PlayerPawn;
            break;
        }

        const float DistanceSquared = FVector::DistSquared(
            ControlledBoss->GetActorLocation(), PlayerPawn->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            NewTarget = PlayerPawn;
        }
    }

    UpdateTargetActor(NewTarget);
}

void ABossAIController::UpdateTargetActor(AActor* NewTarget)
{
    if (!HasAuthority() || !IsValid(ControlledBoss.Get()))
    {
        return;
    }

    APawn* PlayerPawn = Cast<APawn>(NewTarget);
    if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled()
        || ControlledBoss->GetBossState() == EBossState::Dead)
    {
        NewTarget = nullptr;
    }

    AActor* PreviousTarget = ControlledBoss->GetCurrentTarget();
    ControlledBoss->SetCurrentTarget(NewTarget);

    if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
    {
        if (NewTarget)
        {
            BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), NewTarget);
        }
        else
        {
            BlackboardComponent->ClearValue(TEXT("TargetActor"));
            BlackboardComponent->SetValueAsFloat(TEXT("TargetDistance"), TNumericLimits<float>::Max());
            BlackboardComponent->SetValueAsBool(TEXT("bIsInCombatRange"), false);
            BlackboardComponent->SetValueAsBool(TEXT("bIsInAttackRange"), false);
        }
    }

    if (!NewTarget)
    {
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (PreviousTarget != NewTarget)
    {
        UE_LOG(LogTemp, Log,
            TEXT("BossAIController::UpdateTargetActor [%s] - Target: %s -> %s"),
            *GetName(), *GetNameSafe(PreviousTarget), *GetNameSafe(NewTarget));
    }
}

void ABossAIController::UpdateCombatRange(
    EBossAttackType AttackType, float CombatRadius)
{
    if (!HasAuthority())
    {
        return;
    }

    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!IsValid(BB))
    {
        return;
    }

    // Notify BT observers only after all final range values have been written.
    BB->PauseObserverNotifications();
    ON_SCOPE_EXIT
    {
        BB->ResumeObserverNotifications(true);
    };

    BB->SetValueAsFloat(TEXT("TargetDistance"), TNumericLimits<float>::Max());
    BB->SetValueAsBool(TEXT("bIsInCombatRange"), false);
    BB->SetValueAsBool(TEXT("bIsInAttackRange"), false);

    if (!IsValid(ControlledBoss.Get()) || ControlledBoss.Get() != GetPawn())
    {
        return;
    }

    const EBossState State = ControlledBoss->GetBossState();
    BB->SetValueAsEnum(TEXT("BossState"), static_cast<uint8>(State));
    BB->SetValueAsEnum(TEXT("BossPhase"),
        static_cast<uint8>(ControlledBoss->GetBossPhase()));

    AActor* Target = ControlledBoss->GetCurrentTarget();
    APawn* PlayerPawn = Cast<APawn>(Target);
    if (State == EBossState::Dead
        || !IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
    {
        UpdateTargetActor(nullptr);
        return;
    }

    // 3D 중심 간 거리(cm). 높이 차이도 공격 거리 판정에 포함한다.
    const float Distance = FVector::Distance(
        ControlledBoss->GetActorLocation(), PlayerPawn->GetActorLocation());
    if (!FMath::IsFinite(Distance))
    {
        return;
    }
    BB->SetValueAsFloat(TEXT("TargetDistance"), Distance);

    const bool bNormalCombatState = State == EBossState::Idle
        || State == EBossState::Combat || State == EBossState::Attacking;
    const bool bInCombatRange = bNormalCombatState
        && FMath::IsFinite(CombatRadius) && CombatRadius > 0.f
        && Distance <= CombatRadius;

    BB->SetValueAsBool(TEXT("bIsInCombatRange"), bInCombatRange);
    BB->SetValueAsBool(TEXT("bIsInAttackRange"), bInCombatRange
        && ControlledBoss->IsDistanceInAttackRange(AttackType, Distance));
}

void ABossAIController::OnUnPossess()
{
    UpdateTargetActor(nullptr);
    Super::OnUnPossess();
    ControlledBoss = nullptr;
}