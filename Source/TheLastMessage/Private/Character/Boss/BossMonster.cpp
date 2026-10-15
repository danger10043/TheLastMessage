#include "Character/Boss/BossMonster.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Character/Boss/BossAttributeSet.h"
#include "Character/Boss/BossAIController.h"
#include "Character/Boss/BossPhaseComponent.h"
#include "Character/Boss/BossPatternComponent.h"
#include "Character/Boss/BossShieldComponent.h"
#include "Character/Boss/BossCorePuzzleComponent.h"

ABossMonster::ABossMonster()
{
    AIControllerClass = ABossAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    AbilitySystemComponent =
        CreateDefaultSubobject<UAbilitySystemComponent>(
            TEXT("AbilitySystemComponent"));
    AttributeSet =
        CreateDefaultSubobject<UBossAttributeSet>(TEXT("AttributeSet"));
    PhaseComponent =
        CreateDefaultSubobject<UBossPhaseComponent>(TEXT("PhaseComponent"));
    PatternComponent =
        CreateDefaultSubobject<UBossPatternComponent>(TEXT("PatternComponent"));
    ShieldComponent =
        CreateDefaultSubobject<UBossShieldComponent>(TEXT("ShieldComponent"));
    CorePuzzleComponent =
        CreateDefaultSubobject<UBossCorePuzzleComponent>(
            TEXT("CorePuzzleComponent"));
}

void ABossMonster::BeginPlay()
{
    Super::BeginPlay();
    InitializeBoss();
}

UAbilitySystemComponent* ABossMonster::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent.Get();
}

void ABossMonster::InitializeBoss()
{
    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::InitializeBoss [%s] - 보스가 사망한 상태입니다."),
            *GetName());
        return;
    }

    if (bInitializationInProgress)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::InitializeBoss [%s] - 이미 초기화가 진행 중입니다."),
            *GetName());
        return;
    }

    if (!ensureMsgf(IsValid(AbilitySystemComponent.Get()),
        TEXT("Boss AbilitySystemComponent is invalid.")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossMonster::InitializeBoss [%s] - ASC가 유효하지 않습니다."),
            *GetName());
        return;
    }

    if (!ensureMsgf(IsValid(AttributeSet.Get()),
        TEXT("Boss AttributeSet is invalid.")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossMonster::InitializeBoss [%s] - AttributeSet이 유효하지 않습니다."),
            *GetName());
        return;
    }

    TGuardValue<bool> InitializationGuard(bInitializationInProgress, true);

    bool bNeedsActorInfoInitialization = false;

    if (AbilitySystemComponent->GetOwnerActor() != this)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("BossMonster::InitializeBoss [%s] - OwnerActor가 일치하지 않아 초기화합니다. 현재: %s"),
            *GetName(),
            *GetNameSafe(AbilitySystemComponent->GetOwnerActor()));
        bNeedsActorInfoInitialization = true;
    }

    if (AbilitySystemComponent->GetAvatarActor() != this)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("BossMonster::InitializeBoss [%s] - AvatarActor가 일치하지 않아 초기화합니다. 현재: %s"),
            *GetName(),
            *GetNameSafe(AbilitySystemComponent->GetAvatarActor()));
        bNeedsActorInfoInitialization = true;
    }

    if (bNeedsActorInfoInitialization)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    const UBossAttributeSet* RegisteredSet =
        AbilitySystemComponent->GetSet<UBossAttributeSet>();

    if (!RegisteredSet)
    {
        AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
    }

    else if (!ensureMsgf(RegisteredSet == AttributeSet.Get(),
        TEXT("A different BossAttributeSet is already registered.")))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossMonster::InitializeBoss [%s] - 다른 BossAttributeSet이 등록되어 있습니다. 등록값: %s, 기대값: %s"),
            *GetName(),
            *GetNameSafe(RegisteredSet),
            *GetNameSafe(AttributeSet.Get()));
        return;
    }

    // GE 적용과 능력 부여는 권한을 가진 인스턴스에서 수행한다.
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("BossMonster::InitializeBoss [%s] - 권한이 없어 초기 스탯 적용과 능력 부여를 생략합니다."),
            *GetName());
        return;
    }

    if (!bInitialStatsApplied)
    {
        if (!InitialStatsEffect)
        {
            UE_LOG(LogTemp, Error,
                TEXT("%s: InitialStatsEffect is not configured."),
                *GetName());
            return;
        }

        const UGameplayEffect* EffectDefaults =
            InitialStatsEffect.GetDefaultObject();

        if (EffectDefaults->DurationPolicy
            != EGameplayEffectDurationType::Instant)
        {
            UE_LOG(LogTemp, Error,
                TEXT("%s: InitialStatsEffect must be Instant."),
                *GetName());
            return;
        }

        FGameplayEffectContextHandle Context =
            AbilitySystemComponent->MakeEffectContext();
        Context.AddSourceObject(this);

        FGameplayEffectSpecHandle Spec =
            AbilitySystemComponent->MakeOutgoingSpec(
                InitialStatsEffect, 1.f, Context);

        if (!Spec.IsValid())
        {
            UE_LOG(LogTemp, Error,
                TEXT("%s: Failed to create initial stats spec."),
                *GetName());
            return;
        }

        const FActiveGameplayEffectHandle AppliedHandle =
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
                *Spec.Data.Get());

        // Instant GE는 IsValid() 대신 적용 성공 여부를 확인한다.
        if (!AppliedHandle.WasSuccessfullyApplied())
        {
            UE_LOG(LogTemp, Error,
                TEXT("%s: Initial stats effect was rejected."),
                *GetName());
            return;
        }

        bInitialStatsApplied = true;

        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->MaxWalkSpeed =
                FMath::Max(0.f, AttributeSet->GetMoveSpeed());
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("BossMonster::InitializeBoss [%s] - CharacterMovementComponent가 없어 초기 이동속도를 반영하지 못했습니다."),
                *GetName());
        }
    }

    for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
    {
        if (!AbilityClass)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("%s: DefaultAbilities contains an empty entry."),
                *GetName());
            continue;
        }

        if (AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
        {
            UE_LOG(LogTemp, Verbose,
                TEXT("BossMonster::InitializeBoss [%s] - 이미 부여된 능력이므로 생략합니다. 능력: %s"),
                *GetName(),
                *AbilityClass->GetName());
            continue;
        }

        const FGameplayAbilitySpecHandle AbilityHandle =
            AbilitySystemComponent->GiveAbility(
                FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));

        if (!AbilityHandle.IsValid())
        {
            UE_LOG(LogTemp, Error,
                TEXT("%s: Failed to grant %s."),
                *GetName(), *AbilityClass->GetName());
        }
    }

    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::InitializeBoss - 초기화 중 사망하여 페이즈 설정을 생략합니다."));
        return;
    }
    if (IsValid(PhaseComponent.Get()))
    {
        PhaseComponent->ApplyPhaseSettings(PhaseComponent->GetCurrentPhase());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::InitializeBoss - PhaseComponent가 없습니다."));
    }
}

EBossPhase ABossMonster::GetBossPhase()
{
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::GetBossPhase [%s] - PhaseComponent가 유효하지 않아 Phase1을 반환합니다."),
            *GetName());
        return EBossPhase::Phase1;
    }

    return PhaseComponent->GetCurrentPhase();
}

EBossState ABossMonster::GetBossState()
{
    return CurrentState;
}

void ABossMonster::SetBossState(EBossState NewState)
{
    if (NewState == EBossState::Dead)
    {
        HandleBossDeath();
        return;
    }

    if (CurrentState == NewState)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("BossMonster::SetBossState [%s] - 이미 요청한 상태이므로 변경을 생략합니다. 상태: %d"),
            *GetName(),
            static_cast<int32>(NewState));
        return;
    }

    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::SetBossState [%s] - 사망 상태에서는 다른 상태로 변경할 수 없습니다. 요청 상태: %d"),
            *GetName(),
            static_cast<int32>(NewState));
        return;
    }

    CurrentState = NewState;
    OnBossStateChanged.Broadcast(CurrentState);
}

bool ABossMonster::IsRangedImmune()
{
    return bIsRangedImmune;
}

void ABossMonster::SetRangedImmunity(bool bEnabled)
{
    if (!IsValid(AbilitySystemComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::SetRangedImmunity - ASC가 유효하지 않습니다."));
        return;
    }
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::SetRangedImmunity - 실행 권한이 없습니다."));
        return;
    }
    const FGameplayTag RangedTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Boss.Immunity.Ranged")), false);
    if (!RangedTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::SetRangedImmunity - 원거리 면역 태그가 등록되지 않았습니다."));
        return;
    }

    bIsRangedImmune = bEnabled;
    AbilitySystemComponent->SetLooseGameplayTagCount(RangedTag, bEnabled ? 1 : 0);
}

float ABossMonster::GetDamageHealthFloorRatio() const
{
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::GetDamageHealthFloorRatio - PhaseComponent가 없습니다. 피해를 제한합니다."));
        return 1.f;
    }
    return PhaseComponent->GetDamageHealthFloorRatio();
}

void ABossMonster::CheckBossPhaseThreshold()
{
    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::CheckBossPhaseThreshold - 사망한 보스의 전환 요청을 생략합니다."));
        return;
    }
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::CheckBossPhaseThreshold - 실행 권한이 없습니다."));
        return;
    }
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::CheckBossPhaseThreshold - PhaseComponent가 없습니다."));
        return;
    }
    if (!IsValid(AttributeSet.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::CheckBossPhaseThreshold - AttributeSet이 없습니다."));
        return;
    }
    PhaseComponent->CheckPhaseThreshold(AttributeSet->GetHealthPercent());
}

void ABossMonster::StartShieldPuzzle(EBossPuzzleType PuzzleType)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::StartShieldPuzzle - 실행 권한이 없습니다."));
        return;
    }
    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::StartShieldPuzzle - 사망 상태입니다."));
        return;
    }
    if (CurrentState == EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::StartShieldPuzzle - 이미 퍼즐 진행 중입니다."));
        return;
    }
    if (CurrentState == EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::StartShieldPuzzle - 시네마틱 진행 중입니다."));
        return;
    }
    if (!IsValid(ShieldComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::StartShieldPuzzle - ShieldComponent가 없습니다."));
        return;
    }
    if (!IsValid(AbilitySystemComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::StartShieldPuzzle - ASC가 없습니다."));
        return;
    }

    ShieldComponent->ActivateShield(PuzzleType);
    if (!ShieldComponent->IsShieldActive())
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::StartShieldPuzzle - 실드 활성화에 실패했습니다."));
        return;
    }

    // 취소 콜백 전에 실드 면역을 먼저 활성화한다.
    AbilitySystemComponent->CancelAllAbilities();
    
    SetBossState(EBossState::ShieldPuzzle);
    if (CurrentState != EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::StartShieldPuzzle - 상태 변경 콜백에서 퍼즐 진입이 중단되었습니다."));
        return;
    }

    ShieldComponent->StartPuzzleTimer();
    if (ShieldComponent->GetRemainingPuzzleTime() <= 0.f)
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossMonster::StartShieldPuzzle - 타이머 시작에 실패했습니다."));
        ShieldComponent->DeactivateShield();
        SetBossState(EBossState::Combat);
        return;
    }

    OnBossPuzzleStarted.Broadcast(PuzzleType);

    // 레이저 장치 활성화와 코어 생성은 퍼즐 구현 단계에서 연결한다.
}

void ABossMonster::HandleBossDeath()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::HandleBossDeath - 실행 권한이 없습니다."));
        return;
    }
    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::HandleBossDeath - 이미 사망 처리되었습니다."));
        return;
    }

    // 취소·체력 알림·이벤트 콜백보다 먼저 사망 상태를 확정한다.
    CurrentState = EBossState::Dead;
    CurrentTarget = nullptr;
    bIsRangedImmune = false;

    if (IsValid(AbilitySystemComponent.Get()))
    {
        const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Boss.State.Dead")), false);
        if (DeadTag.IsValid())
        {
            AbilitySystemComponent->SetLooseGameplayTagCount(DeadTag, 1);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("BossMonster::HandleBossDeath - Boss.State.Dead 태그가 없습니다. 나머지 정리는 계속합니다."));
        }

        AbilitySystemComponent->CancelAllAbilities();
        if (IsValid(AttributeSet.Get()))
        {
            AbilitySystemComponent->SetNumericAttributeBase(
                UBossAttributeSet::GetHealthAttribute(), 0.f);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BossMonster::HandleBossDeath - AttributeSet이 없어 체력 0 반영을 생략합니다."));
        }
        SetRangedImmunity(false);
    }
    else
    {
        bIsRangedImmune = false;
        UE_LOG(LogTemp, Error, TEXT("BossMonster::HandleBossDeath - ASC가 없습니다. 나머지 정리는 계속합니다."));
    }

    if (IsValid(PatternComponent.Get()))
    {
        PatternComponent->CancelCurrentAttack();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::HandleBossDeath - PatternComponent가 없습니다."));
    }

    AController* BossController = GetController();
    if (IsValid(BossController))
    {
        BossController->StopMovement();
        if (AAIController* AI = Cast<AAIController>(BossController))
        {
            if (UBrainComponent* Brain = AI->GetBrainComponent())
            {
                Brain->StopLogic(TEXT("Boss died"));
            }
        }
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::HandleBossDeath - 이동 컴포넌트가 없습니다."));
    }
    StopAnimMontage();
    SetActorEnableCollision(false);

    if (IsValid(ShieldComponent.Get()))
    {
        ShieldComponent->DeactivateShield();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::HandleBossDeath - ShieldComponent가 없습니다."));
    }
    if (IsValid(CorePuzzleComponent.Get()))
    {
        CorePuzzleComponent->EndCorePuzzle();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::HandleBossDeath - CorePuzzleComponent가 없습니다."));
    }

    // 정리 후에 알린다. Actor를 자동으로 Destroy하지 않는다.
    OnBossStateChanged.Broadcast(EBossState::Dead);
    OnBossDied.Broadcast();
}

void ABossMonster::CompleteShieldPuzzle()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::CompleteShieldPuzzle - 실행 권한이 없습니다."));
        return;
    }
    if (CurrentState != EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::CompleteShieldPuzzle - 실드 퍼즐 진행 중이 아닙니다."));
        return;
    }
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::CompleteShieldPuzzle - PhaseComponent가 없습니다."));
        return;
    }
    if (!IsValid(ShieldComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::CompleteShieldPuzzle - ShieldComponent가 없습니다."));
        return;
    }
    if (!ShieldComponent->IsShieldActive())
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::CompleteShieldPuzzle - 실드가 활성 상태가 아닙니다."));
        return;
    }

    EBossPhase NextPhase;
    if (!PhaseComponent->TryGetPendingPhase(NextPhase))
    {
        return;
    }
    EBossPuzzleType PuzzleType;
    switch (NextPhase)
    {
    case EBossPhase::Phase2:
        PuzzleType = EBossPuzzleType::LaserPuzzle;
        break;
    case EBossPhase::Phase3:
        PuzzleType = EBossPuzzleType::CorePuzzle;
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::CompleteShieldPuzzle - 잘못된 다음 페이즈입니다."));
        return;
    }

    // 실드 해제 전에 Cinematic으로 변경하여 피해 보호를 유지한다.
    StartPhaseCinematic(NextPhase);
    if (CurrentState != EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::CompleteShieldPuzzle - 시네마틱 대기 상태로 진입하지 못했습니다."));
        return;
    }
    ShieldComponent->DeactivateShield();
    if (IsValid(CorePuzzleComponent.Get()))
    {
        CorePuzzleComponent->EndCorePuzzle();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::CompleteShieldPuzzle - CorePuzzleComponent가 없습니다."));
    }
    if (CurrentState != EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::CompleteShieldPuzzle - 정리 중 전환이 중단되었습니다."));
        return;
    }
    OnBossPuzzleCompleted.Broadcast(PuzzleType);
}

void ABossMonster::StartPhaseCinematic(EBossPhase NextPhase)
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::StartPhaseCinematic - 실행 권한이 없습니다."));
        return;
    }
    if (CurrentState != EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::StartPhaseCinematic - 실드 퍼즐 상태가 아닙니다."));
        return;
    }
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::StartPhaseCinematic - PhaseComponent가 없습니다."));
        return;
    }
    EBossPhase Pending;
    if (!PhaseComponent->TryGetPendingPhase(Pending))
    {
        return;
    }
    if (Pending != NextPhase)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::StartPhaseCinematic - 대기 페이즈와 요청이 다릅니다."));
        return;
    }

    SetBossState(EBossState::Cinematic);
    if (CurrentState != EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::StartPhaseCinematic - 상태 변경 콜백에서 진행이 중단되었습니다."));
        return;
    }
    if (IsValid(PatternComponent.Get()))
    {
        PatternComponent->CancelCurrentAttack();
    }

    if (AController* BossController = GetController())
    {
        BossController->StopMovement();
        if (AAIController* AI = Cast<AAIController>(BossController))
        {
            if (UBrainComponent* Brain = AI->GetBrainComponent())
            {
                Brain->PauseLogic(TEXT("Boss phase cinematic"));
            }
        }
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    // 현재는 시네마틱 대기 상태만 만든다.
    // 실제 Sequence 종료 이벤트에서 FinishPhaseCinematic을 호출하도록 연결한다.
}

void ABossMonster::FinishPhaseCinematic()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::FinishPhaseCinematic - 실행 권한이 없습니다."));
        return;
    }
    if (CurrentState != EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FinishPhaseCinematic - 시네마틱 상태가 아닙니다."));
        return;
    }
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::FinishPhaseCinematic - PhaseComponent가 없습니다."));
        return;
    }
    if (!IsValid(ShieldComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::FinishPhaseCinematic - ShieldComponent가 없습니다."));
        return;
    }
    if (ShieldComponent->IsShieldActive())
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FinishPhaseCinematic - 실드 성공 정리가 아직 완료되지 않았습니다."));
        return;
    }
    EBossPhase NextPhase;
    if (!PhaseComponent->TryGetPendingPhase(NextPhase))
    {
        return;
    }

    PhaseComponent->CommitPhaseTransition();
    if (PhaseComponent->GetCurrentPhase() != NextPhase)
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::FinishPhaseCinematic - 페이즈 확정에 실패했습니다."));
        return;
    }
    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::FinishPhaseCinematic - 전환 중 사망했습니다."));
        return;
    }

    SetBossState(EBossState::Combat);
    if (CurrentState != EBossState::Combat)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FinishPhaseCinematic - 상태 변경 콜백에서 전투 복귀가 중단되었습니다."));
        return;
    }
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AI->GetBrainComponent())
        {
            Brain->ResumeLogic(TEXT("Boss phase cinematic finished"));
        }
    }
    OnBossPhaseChanged.Broadcast(NextPhase);
}

void ABossMonster::FailShieldPuzzle()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::FailShieldPuzzle - 실행 권한이 없습니다."));
        return;
    }
    if (CurrentState != EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FailShieldPuzzle - 실드 퍼즐 진행 중이 아닙니다."));
        return;
    }
    if (!IsValid(PhaseComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::FailShieldPuzzle - PhaseComponent가 없습니다."));
        return;
    }
    if (!IsValid(ShieldComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::FailShieldPuzzle - ShieldComponent가 없습니다."));
        return;
    }
    if (!IsValid(CorePuzzleComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster::FailShieldPuzzle - CorePuzzleComponent가 없습니다."));
        return;
    }
    if (!ShieldComponent->IsShieldActive())
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FailShieldPuzzle - 실드가 이미 해제되었습니다."));
        return;
    }
    if (!ShieldComponent->CanApplyFailureRecovery())
    {
        return;
    }

    EBossPhase NextPhase;
    if (!PhaseComponent->TryGetPendingPhase(NextPhase))
    {
        return;
    }
    EBossPuzzleType PuzzleType;
    switch (NextPhase)
    {
    case EBossPhase::Phase2:
        PuzzleType = EBossPuzzleType::LaserPuzzle;
        break;
    case EBossPhase::Phase3:
        PuzzleType = EBossPuzzleType::CorePuzzle;
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FailShieldPuzzle - 잘못된 대기 페이즈입니다."));
        return;
    }

    // 실드 해제 과정에서 타이머도 중단된다.
    ShieldComponent->DeactivateShield();
    CorePuzzleComponent->ResetCorePuzzle();
    if (!ShieldComponent->TryApplyFailureRecovery())
    {
        if (CurrentState != EBossState::Dead)
        {
            ShieldComponent->ActivateShield(PuzzleType);
            UE_LOG(LogTemp, Error, TEXT("BossMonster::FailShieldPuzzle - 회복에 실패하여 전투 복귀를 중단했습니다. 오류를 확인하고 테스트를 재시작하세요."));
        }
        return;
    }
    PhaseComponent->CancelPhaseTransition();

    if (CurrentState == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::FailShieldPuzzle - 실패 정리 중 사망하여 전투에 복귀하지 않습니다."));
        return;
    }
    SetBossState(EBossState::Combat);
    if (CurrentState == EBossState::Combat)
    {
        if (AAIController* AI = Cast<AAIController>(GetController()))
        {
            if (UBrainComponent* Brain = AI->GetBrainComponent())
            {
                Brain->ResumeLogic(TEXT("Boss puzzle failed"));
            }
        }
    }
    if (CurrentState == EBossState::Dead)
    {
        return;
    }
    OnBossPuzzleFailed.Broadcast(PuzzleType);
}

float ABossMonster::GetRemainingPuzzleTime() const
{
    if (!IsValid(ShieldComponent.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::GetRemainingPuzzleTime - ShieldComponent가 없습니다."));
        return 0.f;
    }
    return ShieldComponent->GetRemainingPuzzleTime();
}

bool ABossMonster::IsDistanceInAttackRange(
    EBossAttackType AttackType, float TargetDistance) const
{
    return IsValid(PatternComponent.Get())
        && PatternComponent->IsDistanceInAttackRange(
            AttackType, TargetDistance);
}

AActor* ABossMonster::GetCurrentTarget()
{
    return IsValid(CurrentTarget.Get()) ? CurrentTarget.Get() : nullptr;
}

void ABossMonster::SetCurrentTarget(AActor* NewTarget)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentTarget = CurrentState != EBossState::Dead && IsValid(NewTarget)
        ? NewTarget : nullptr;
}