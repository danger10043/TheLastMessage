#include "Character/Boss/BossAIController.h"

#include "Character/Boss/BossMonster.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "Character/Player/PlayerBase.h"
#include "AbilitySystemComponent.h"
#include "Engine/TriggerBox.h"
#include "Components/BoxComponent.h"

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
    BlackboardComponent->SetValueAsBool(TEXT("bCanUseCircularMissile"), false);
    BlackboardComponent->SetValueAsBool(TEXT("bNeedsTurn180"), false);
    BlackboardComponent->SetValueAsBool(TEXT("bIsTurning"), false);
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
    if (!HasAuthority() || !IsValid(ControlledBoss.Get())
        || ControlledBoss.Get() != GetPawn()
        || !IsValid(GetWorld()) || !IsValid(GetBlackboardComponent()))
    {
        return;
    }

    ATriggerBox* DetectionBox = ControlledBoss->TargetDetectionBox.Get();
    const UBoxComponent* DetectionComponent = IsValid(DetectionBox)
        ? Cast<UBoxComponent>(DetectionBox->GetCollisionComponent())
        : nullptr;

    if (ControlledBoss->GetBossState() == EBossState::Dead
        || !IsValid(DetectionComponent))
    {
        UpdateTargetActor(nullptr);
        return;
    }

    const FTransform BoxTransform = DetectionComponent->GetComponentTransform();
    const FVector BoxExtent = DetectionComponent->GetUnscaledBoxExtent();

    if (BoxTransform.ContainsNaN() || BoxExtent.ContainsNaN()
        || BoxExtent.GetMin() <= 0.f
        || BoxTransform.GetScale3D().GetAbsMin() <= KINDA_SMALL_NUMBER)
    {
        UpdateTargetActor(nullptr);
        return;
    }

    const FVector BossLocation = ControlledBoss->GetActorLocation();

    AActor* CurrentTarget = ControlledBoss->GetCurrentTarget();
    AActor* NewTarget = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (TActorIterator<APlayerBase> It(GetWorld()); It; ++It)
    {
        APlayerBase* PlayerPawn = *It;
        if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled()
            || PlayerPawn->IsDead())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(
            BossLocation, PlayerPawn->GetActorLocation());

        const FVector LocalPlayerLocation =
            BoxTransform.InverseTransformPosition(PlayerPawn->GetActorLocation());

        const bool bInsideDetectionBox =
            FMath::Abs(LocalPlayerLocation.X) <= BoxExtent.X
            && FMath::Abs(LocalPlayerLocation.Y) <= BoxExtent.Y
            && FMath::Abs(LocalPlayerLocation.Z) <= BoxExtent.Z;

        if (!FMath::IsFinite(DistanceSquared) || !bInsideDetectionBox)
        {
            continue;
        }

        if (PlayerPawn == CurrentTarget)
        {
            NewTarget = PlayerPawn;
            break;
        }

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

    APlayerBase* PlayerPawn = Cast<APlayerBase>(NewTarget);
    if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled()
        || PlayerPawn->IsDead()
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

    if (ControlledBoss->GetBossState() != EBossState::Dead)
    {
        RefreshTargetFromPerception();
    }

    const EBossState State = ControlledBoss->GetBossState();
    BB->SetValueAsEnum(TEXT("BossState"), static_cast<uint8>(State));
    BB->SetValueAsEnum(TEXT("BossPhase"),
        static_cast<uint8>(ControlledBoss->GetBossPhase()));

    AActor* Target = ControlledBoss->GetCurrentTarget();
    APlayerBase* PlayerPawn = Cast<APlayerBase>(Target);
    if (State == EBossState::Dead
        || !IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled()
        || PlayerPawn->IsDead())
    {
        UpdateTargetActor(nullptr);

        if (State == EBossState::Combat || State == EBossState::Attacking)
        {
            if (UAbilitySystemComponent* ASC =
                ControlledBoss->GetAbilitySystemComponent())
            {
                ASC->CancelAllAbilities();
            }

            if (IsValid(ControlledBoss.Get())
                && (ControlledBoss->GetBossState() == EBossState::Combat
                    || ControlledBoss->GetBossState() == EBossState::Attacking))
            {
                ControlledBoss->StopAnimMontage();
                ControlledBoss->SetBossState(EBossState::Idle);
            }
        }
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

    const bool bCrossLaserInProgress =
        State == EBossState::Attacking
        && BB->GetValueAsEnum(TEXT("SelectedAttack"))
        == static_cast<uint8>(EBossAttackType::CrossLaser);

    const bool bInCombatRange = bNormalCombatState
        && (bCrossLaserInProgress
            || (FMath::IsFinite(CombatRadius) && CombatRadius > 0.f
                && Distance <= CombatRadius));

    BB->SetValueAsBool(TEXT("bIsInCombatRange"), bInCombatRange);
    BB->SetValueAsBool(TEXT("bIsInAttackRange"), bInCombatRange
        && (bCrossLaserInProgress
            || ControlledBoss->IsDistanceInAttackRange(AttackType, Distance)));
}

void ABossAIController::OnUnPossess()
{
    UpdateTargetActor(nullptr);
    Super::OnUnPossess();
    ControlledBoss = nullptr;
}