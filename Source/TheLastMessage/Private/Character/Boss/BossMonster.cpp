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
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Laser/LaserEmitter.h"
#include "Gimmick/Laser/LaserReceiver.h"
#include "Gimmick/Laser/LaserReflector.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Character/Boss/BossActivationCore.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "EngineUtils.h"
#include "Framework/ActorPoolSubsystem.h"
#include "Interface/PoolableInterface.h"

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

    GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
    GunMesh->SetupAttachment(GetMesh(), TEXT("Gun_Socket"));
    GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GunMesh->SetGenerateOverlapEvents(false);

    ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
    ShieldMesh->SetupAttachment(GetMesh(), TEXT("Shield_Socket"));
    ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShieldMesh->SetGenerateOverlapEvents(false);

    PuzzleShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("PuzzleShieldMesh"));
    PuzzleShieldMesh->SetupAttachment(GetRootComponent());
    PuzzleShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PuzzleShieldMesh->SetGenerateOverlapEvents(false);
    PuzzleShieldMesh->SetCanEverAffectNavigation(false);
    PuzzleShieldMesh->SetHiddenInGame(true);
    PuzzleShieldMesh->SetVisibility(false);

    PrimaryActorTick.bCanEverTick = true;
}

void ABossMonster::UpdateEquipmentForPhase(EBossPhase Phase)
{
    const bool bShowShield = Phase == EBossPhase::Phase1;
    const bool bShowGun = Phase == EBossPhase::Phase1
        || Phase == EBossPhase::Phase2;

    ShieldMesh->SetVisibility(bShowShield, true);
    ShieldMesh->SetHiddenInGame(!bShowShield, true);

    GunMesh->SetVisibility(bShowGun, true);
    GunMesh->SetHiddenInGame(!bShowGun, true);
}

void ABossMonster::BeginPlay()
{
    Super::BeginPlay();
    InitialTransform = GetActorTransform();
    UpdateEquipmentForPhase(GetBossPhase());
    InitializeBoss();

    if (IsValid(AbilitySystemComponent.Get())
        && CurrentState != EBossState::Dead)
    {
        HealthChangedDelegateHandle = AbilitySystemComponent
            ->GetGameplayAttributeValueChangeDelegate(
                UBossAttributeSet::GetHealthAttribute())
            .AddUObject(this, &ABossMonster::HandleBossHealthChanged);
    }
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

bool ABossMonster::ResetBoss(bool bRestartAI)
{
    UWorld* World = GetWorld();
    ABossAIController* AI = Cast<ABossAIController>(GetController());
    UCharacterMovementComponent* Movement = GetCharacterMovement();

    if (!HasAuthority() || !HasActorBegunPlay() || IsActorBeingDestroyed()
        || bIsResettingBoss || bInitializationInProgress || !bInitialStatsApplied
        || !World || !IsValid(AI) || !IsValid(Movement)
        || !IsValid(GetMesh()) || !IsValid(AbilitySystemComponent.Get())
        || !IsValid(AttributeSet.Get()) || !IsValid(PhaseComponent.Get())
        || !IsValid(PatternComponent.Get()) || !IsValid(ShieldComponent.Get())
        || !IsValid(CorePuzzleComponent.Get())
        || !IsValid(PuzzleShieldMesh.Get()) || !IsValid(GunMesh.Get())
        || !IsValid(ShieldMesh.Get()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResetBoss: authority, initialization or component check failed."));
        return false;
    }

    const float MaxHealth = AttributeSet->GetMaxHealth();
    const float MoveSpeed = AttributeSet->GetMoveSpeed();
    UActorPoolSubsystem* Pool = World->GetSubsystem<UActorPoolSubsystem>();
    if (!IsValid(Pool) || !FMath::IsFinite(MaxHealth) || MaxHealth <= 0.f
        || !FMath::IsFinite(MoveSpeed) || MoveSpeed < 0.f)
    {
        return false;
    }

    const TCHAR* RequiredTags[] = {
        TEXT("Boss.State.Dead"), TEXT("Boss.Immunity.Shield"),
        TEXT("Boss.Immunity.Ranged"), TEXT("Boss.Phase.One"),
        TEXT("Boss.Phase.Two"), TEXT("Boss.Phase.Three")
    };
    for (const TCHAR* Name : RequiredTags)
    {
        if (!FGameplayTag::RequestGameplayTag(FName(Name), false).IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("ResetBoss: missing tag %s"), Name);
            return false;
        }
    }

    TGuardValue<bool> ResetGuard(bIsResettingBoss, true);

    // 사망 상태에서도 초기화를 허용한다. 중간 콜백에는 전투 불가 상태를 노출한다.
    CurrentState = EBossState::Cinematic;
    if (UBrainComponent* Brain = AI->GetBrainComponent())
    {
        if (UBehaviorTreeComponent* BT = Cast<UBehaviorTreeComponent>(Brain))
        {
            BT->StopTree(EBTStopMode::Forced);
        }
        else
        {
            Brain->StopLogic(TEXT("Boss reset"));
        }
    }
    AI->StopMovement();
    StopPhaseSequence();
    World->GetTimerManager().ClearAllTimersForObject(this);
    EndHitFlash();

    // 진행 중인 공격 GA와 레이저 퍼즐 기믹도 정리한다. 상태 변경은 ResetGuard가 막는다.
    AbilitySystemComponent->CancelAllAbilities();
    StopLaserPuzzle();
    SetRangedImmunity(false);

    PatternComponent->ResetPattern();
    ShieldComponent->DeactivateShield();
    CorePuzzleComponent->EndCorePuzzle();
    for (ABossActivationCore* Core : ActivationCores)
    {
        if (IsValid(Core))
        {
            Core->ResetCore();
        }
    }

    // 취소 후에도 남은, 이 보스 소유의 풀링 액터만 회수한다.
    TArray<TWeakObjectPtr<AActor>> RemainingActors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (IsValid(Actor) && Actor->GetOwner() == this
            && Cast<IPoolableInterface>(Actor))
        {
            RemainingActors.Add(Actor);
        }
    }
    for (const TWeakObjectPtr<AActor>& Entry : RemainingActors)
    {
        if (AActor* Actor = Entry.Get())
        {
            if (Actor->GetOwner() == this)
            {
                Pool->ReleaseActor(Actor);
            }
        }
    }

    GetMesh()->bPauseAnims = false;
    if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
    {
        Anim->StopAllMontages(0.f);
    }
    PuzzleMontageInstanceID = INDEX_NONE;
    ActivePuzzleMontage = nullptr;
    PuzzleAnimInstance.Reset();
    bPuzzlePresentationActive = false;
    bPuzzleExitPending = false;
    PuzzleShieldMesh->SetVisibility(false, true);
    PuzzleShieldMesh->SetHiddenInGame(true, true);
    bGunBurstAiming = false;
    GunBurstAimAngle = 0.f;

    const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Boss.State.Dead")), false);
    AbilitySystemComponent->SetLooseGameplayTagCount(DeadTag, 0);
    AbilitySystemComponent->SetNumericAttributeBase(
        UBossAttributeSet::GetIncomingDamageAttribute(), 0.f);
    AbilitySystemComponent->SetNumericAttributeBase(
        UBossAttributeSet::GetHealthAttribute(), MaxHealth);
    PhaseComponent->ResetPhase();
    UpdateEquipmentForPhase(EBossPhase::Phase1);
    AI->UpdateTargetActor(nullptr);

    Movement->StopMovementImmediately();
    Movement->MaxWalkSpeed = MoveSpeed;
    Movement->SetMovementMode(MOVE_Walking);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
    CurrentState = EBossState::Idle;

    if (UBlackboardComponent* BB = AI->GetBlackboardComponent())
    {
        BB->SetValueAsEnum(TEXT("BossState"), static_cast<uint8>(EBossState::Idle));
        BB->SetValueAsEnum(TEXT("BossPhase"), static_cast<uint8>(EBossPhase::Phase1));
        BB->SetValueAsEnum(TEXT("SelectedAttack"), static_cast<uint8>(EBossAttackType::None));
        BB->SetValueAsEnum(TEXT("PuzzleType"), static_cast<uint8>(EBossPuzzleType::None));
        const TCHAR* BoolKeys[] = {
            TEXT("bIsInCombatRange"), TEXT("bIsInAttackRange"),
            TEXT("bCanUseCircularMissile"), TEXT("bNeedsTurn180"),
            TEXT("bIsTurning"), TEXT("bIsShieldActive"), TEXT("bIsPuzzleActive")
        };
        for (const TCHAR* Key : BoolKeys)
        {
            BB->SetValueAsBool(FName(Key), false);
        }
        BB->ResumeObserverNotifications(false);
    }

    UpdateBossUI();
    OnBossPhaseChanged.Broadcast(EBossPhase::Phase1);
    OnBossStateChanged.Broadcast(EBossState::Idle);

    if (bRestartAI)
    {
        AI->StartBossAI();
        if (UBrainComponent* Brain = AI->GetBrainComponent())
        {
            Brain->ResumeLogic(TEXT("Boss reset completed"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("ResetBoss completed: %s, RestartAI=%d"),
        *GetName(), bRestartAI);
    return true;
}

bool ABossMonster::ResetBossForCheckpoint()
{
    if (!HasAuthority() || InitialTransform.ContainsNaN())
    {
        return false;
    }

    if (!TeleportTo(InitialTransform.GetLocation(), InitialTransform.Rotator(), false, true))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ResetBossForCheckpoint: failed to teleport %s to its initial transform."),
            *GetName());
        return false;
    }

    return ResetBoss(true);
}

FBossMissileConfig ABossMonster::GetMissileConfig(EBossPhase Phase)
{
    if (!IsValid(PatternComponent.Get()))
    {
        UE_LOG(LogTemp, Error, TEXT("BossMonster: PatternComponent is missing."));
        return FBossMissileConfig{};
    }
    return PatternComponent->GetMissileConfig(Phase);
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
    if (bIsResettingBoss)
    {
        return;
    }

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

    if (CurrentState == EBossState::ShieldPuzzle)
    {
        if (AController* BossController = GetController())
        {
            BossController->StopMovement();
        }
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }

    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        if (UBlackboardComponent* BB = AI->GetBlackboardComponent())
        {
            BB->SetValueAsEnum(TEXT("BossState"),
                static_cast<uint8>(CurrentState));
        }
    }

    UpdatePuzzlePresentation();

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

    if (PuzzleType == EBossPuzzleType::CorePuzzle
        && (!IsValid(CorePuzzleComponent.Get())
            || !CorePuzzleComponent->CanStartCorePuzzle(ActivationCores)))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Boss core puzzle: check placed cores and their animation settings."));
        return;
    }

    if (PuzzleType == EBossPuzzleType::LaserPuzzle && !CanStartLaserPuzzle())
    {
        UE_LOG(LogTemp, Error,
            TEXT("Boss laser puzzle: check placed actors and emitter configs."));
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

    if (PuzzleType == EBossPuzzleType::CorePuzzle
        && !CorePuzzleComponent->StartCorePuzzle(ActivationCores))
    {
        FailShieldPuzzle();
        return;
    }

    if (PuzzleType == EBossPuzzleType::LaserPuzzle && !StartLaserPuzzle())
    {
        FailShieldPuzzle();
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
    StopPhaseSequence();
    EndHitFlash();
    UpdatePuzzlePresentation();
    CurrentTarget = nullptr;
    RemoveBossUI();
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

    PlayDeathMontage();
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
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::CompleteShieldPuzzle - Boss=%s State=%s ExitPending=%d MontageID=%d"),
            *GetPathName(),
            *StaticEnum<EBossState>()->GetNameStringByValue(
                static_cast<int64>(CurrentState)),
            bPuzzleExitPending,
            PuzzleMontageInstanceID);
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

    if (!IsActorBeingDestroyed()
        && CurrentState == EBossState::Cinematic)
    {
        PlayPhaseSequence(PuzzleType);
    }
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

void ABossMonster::PlayPhaseSequence(EBossPuzzleType PuzzleType)
{
    if (!HasAuthority() || CurrentState != EBossState::Cinematic
        || bPhaseSequencePlaying)
    {
        return;
    }

    ALevelSequenceActor* SequenceActor = nullptr;
    switch (PuzzleType)
    {
    case EBossPuzzleType::LaserPuzzle:
        SequenceActor = Phase70SequenceActor.Get();
        break;
    case EBossPuzzleType::CorePuzzle:
        SequenceActor = Phase30SequenceActor.Get();
        break;
    default:
        break;
    }

    ULevelSequencePlayer* Player = IsValid(SequenceActor)
        ? SequenceActor->GetSequencePlayer() : nullptr;

    if (!IsValid(Player) || !IsValid(Player->GetSequence())
        || Player->IsPlaying() || Player->IsPaused())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss phase sequence is unavailable or already in use: %s"),
            *GetNameSafe(SequenceActor));
        FinishPhaseCinematic();
        return;
    }

    // 기존 그로기 종료 몽타주가 시퀀스 애니메이션과 겹치지 않게 한다.
    if (UAnimInstance* Anim = PuzzleAnimInstance.Get())
    {
        if (IsValid(ActivePuzzleMontage.Get()))
        {
            Anim->Montage_Stop(0.f, ActivePuzzleMontage.Get());
        }
    }

    if (CurrentState != EBossState::Cinematic || IsActorBeingDestroyed())
    {
        return;
    }

    ActivePhaseSequencePlayer = Player;
    bPhaseSequencePlaying = true;
    Player->OnFinished.AddUniqueDynamic(
        this, &ABossMonster::HandlePhaseSequenceFinished);
    Player->OnStop.AddUniqueDynamic(
        this, &ABossMonster::HandlePhaseSequenceFinished);

    UE_LOG(LogTemp, Log, TEXT("Boss phase sequence started: %s"),
        *GetNameSafe(SequenceActor));
    Player->Play();
}

void ABossMonster::HandlePhaseSequenceFinished()
{
    if (!bPhaseSequencePlaying || bIsResettingBoss)
    {
        return;
    }

    StopPhaseSequence();

    if (HasAuthority() && CurrentState == EBossState::Cinematic
        && !IsActorBeingDestroyed())
    {
        UE_LOG(LogTemp, Log, TEXT("Boss phase sequence ended."));
        FinishPhaseCinematic();
    }
}

void ABossMonster::StopPhaseSequence()
{
    ULevelSequencePlayer* Player = ActivePhaseSequencePlayer.Get();
    ActivePhaseSequencePlayer = nullptr;
    bPhaseSequencePlaying = false;

    if (IsValid(Player))
    {
        Player->OnFinished.RemoveDynamic(
            this, &ABossMonster::HandlePhaseSequenceFinished);
        Player->OnStop.RemoveDynamic(
            this, &ABossMonster::HandlePhaseSequenceFinished);

        if (Player->IsPlaying() || Player->IsPaused())
        {
            Player->Stop();
        }
    }
}

void ABossMonster::FinishPhaseCinematic()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossMonster::FinishPhaseCinematic - 실행 권한이 없습니다."));
        return;
    }
    if (bPhaseSequencePlaying || bIsResettingBoss)
    {
        return;
    }
    if (CurrentState != EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::FinishPhaseCinematic - Boss=%s State=%s ExitPending=%d MontageID=%d"),
            *GetPathName(),
            *StaticEnum<EBossState>()->GetNameStringByValue(
                static_cast<int64>(CurrentState)),
            bPuzzleExitPending,
            PuzzleMontageInstanceID);
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
        UE_LOG(LogTemp, Warning,
            TEXT("BossMonster::FinishPhaseCinematic - 실드 정리가 아직 완료되지 않았습니다."));
        return;
    }

    if (bPuzzleExitPending)
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick(
            this, &ABossMonster::FinishPhaseCinematic);
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

    UpdateEquipmentForPhase(NextPhase);

    SetBossState(EBossState::Combat);
    if (CurrentState != EBossState::Combat)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossMonster::FinishPhaseCinematic - 상태 변경 콜백에서 전투 복귀가 중단되었습니다."));
        return;
    }
    ResumeCombatAfterPuzzleBlend();
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
        ResumeCombatAfterPuzzleBlend();
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

float ABossMonster::GetAttackCooldownRemaining(EBossAttackType AttackType) const
{
    return IsValid(PatternComponent.Get())
        ? PatternComponent->GetAttackCooldownRemaining(AttackType)
        : -1.f;
}

bool ABossMonster::TryStartAttackCooldown(EBossAttackType AttackType)
{
    return HasAuthority() && IsValid(PatternComponent.Get())
        && PatternComponent->TryStartAttackCooldown(AttackType);
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

    CurrentTarget = CurrentState != EBossState::Dead && IsValid(NewTarget) ? NewTarget : nullptr;
    UpdateBossUI();
}

void ABossMonster::UpdatePuzzlePresentation()
{
    const bool bShouldShow = CurrentState == EBossState::ShieldPuzzle;
    if (bPuzzlePresentationActive == bShouldShow
        && CurrentState != EBossState::Dead)
    {
        return;
    }

    bPuzzlePresentationActive = bShouldShow;
    PuzzleShieldMesh->SetVisibility(bShouldShow, true);
    PuzzleShieldMesh->SetHiddenInGame(!bShouldShow, true);

    if (bShouldShow)
    {
        bPuzzleExitPending = false;
        PuzzleMontageInstanceID = INDEX_NONE;
        PuzzleShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PuzzleShieldMesh->SetGenerateOverlapEvents(false);

        if (AAIController* AI = Cast<AAIController>(GetController()))
        {
            if (UBrainComponent* Brain = AI->GetBrainComponent())
            {
                Brain->PauseLogic(TEXT("Boss shield puzzle"));
            }
        }

        UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
        UAnimMontage* Montage = PuzzleGroggyMontage.Get();
        const FName StartSection(TEXT("GroggyStart"));
        const FName EndSection(TEXT("GroggyEnd"));

        if (!Anim || !Montage
            || Montage->GetSectionIndex(StartSection) == INDEX_NONE
            || Montage->GetSectionIndex(PuzzleLoopSection) == INDEX_NONE
            || Montage->GetSectionIndex(EndSection) == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Boss puzzle: GroggyStart, Loop and GroggyEnd sections are required."));
            return;
        }

        if (Anim->Montage_Play(Montage) > 0.f)
        {
            FAnimMontageInstance* Instance =
                Anim->GetActiveInstanceForMontage(Montage);
            if (!Instance)
            {
                Anim->Montage_Stop(0.f, Montage);
                UE_LOG(LogTemp, Warning,
                    TEXT("Boss puzzle: montage instance was not created."));
                return;
            }

            ActivePuzzleMontage = Montage;
            PuzzleAnimInstance = Anim;
            PuzzleMontageInstanceID = Instance->GetInstanceID();

            Anim->Montage_SetNextSection(
                StartSection, PuzzleLoopSection, Montage);
            Anim->Montage_SetNextSection(
                PuzzleLoopSection, PuzzleLoopSection, Montage);
            Anim->Montage_SetNextSection(
                EndSection, NAME_None, Montage);
            Anim->Montage_JumpToSection(StartSection, Montage);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Boss puzzle: groggy montage failed to play."));
        }
        return;
    }

    UAnimInstance* Anim = PuzzleAnimInstance.Get();
    UAnimMontage* Montage = ActivePuzzleMontage.Get();

    if (CurrentState == EBossState::Dead)
    {
        bPuzzleExitPending = false;
        PuzzleMontageInstanceID = INDEX_NONE;
        ActivePuzzleMontage = nullptr;
        PuzzleAnimInstance.Reset();

        if (Anim && Montage)
        {
            Anim->Montage_Stop(0.f, Montage);
        }
        return;
    }

    bPuzzleExitPending = true;

    if (Anim && Montage
        && PuzzleMontageInstanceID != INDEX_NONE
        && Anim->GetMontageInstanceForID(PuzzleMontageInstanceID)
        && !Anim->Montage_GetIsStopped(Montage))
    {
        const FName EndSection(TEXT("GroggyEnd"));
        if (Montage->GetSectionIndex(EndSection) != INDEX_NONE)
        {
            Anim->Montage_SetNextSection(
                PuzzleLoopSection, EndSection, Montage);
            Anim->Montage_SetNextSection(
                EndSection, NAME_None, Montage);
            Anim->Montage_JumpToSection(EndSection, Montage);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Boss puzzle: GroggyEnd is missing; stopping montage."));
            Anim->Montage_Stop(
                FMath::Max(0.f, PuzzleGroggyBlendOutTime), Montage);
        }
    }
}

void ABossMonster::ResumeCombatAfterPuzzleBlend()
{
    if (!HasAuthority() || CurrentState != EBossState::Combat
        || bPuzzleExitPending)
    {
        return;
    }

    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AI->GetBrainComponent())
        {
            Brain->ResumeLogic(TEXT("Boss puzzle presentation finished"));
        }
    }
}

void ABossMonster::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    CheckLaserPuzzleCompletion();
    UpdateBossUI();

    if (bPuzzlePresentationActive)
    {
        PuzzleShieldMesh->AddLocalRotation(
            FRotator(0.f, PuzzleShieldYawSpeed * DeltaSeconds, 0.f));
        return;
    }
    if (!bPuzzleExitPending)
    {
        return;
    }

    if (UAnimInstance* Anim = PuzzleAnimInstance.Get())
    {
        if (PuzzleMontageInstanceID != INDEX_NONE
            && Anim->GetMontageInstanceForID(PuzzleMontageInstanceID))
        {
            return;
        }
    }

    PuzzleMontageInstanceID = INDEX_NONE;
    ActivePuzzleMontage = nullptr;
    PuzzleAnimInstance.Reset();

    bPuzzleExitPending = false;
    if (HasAuthority() && CurrentState == EBossState::Combat)
    {
        ResumeCombatAfterPuzzleBlend();
    }

}

void ABossMonster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopPhaseSequence();

    if (IsValid(AbilitySystemComponent.Get())
        && HealthChangedDelegateHandle.IsValid())
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UBossAttributeSet::GetHealthAttribute())
            .Remove(HealthChangedDelegateHandle);
    }
    HealthChangedDelegateHandle.Reset();
    EndHitFlash();

    RemoveBossUI();
    StopLaserPuzzle();

    if (UAnimInstance* Anim = PuzzleAnimInstance.Get())
    {
        if (ActivePuzzleMontage)
        {
            Anim->Montage_Stop(0.f, ActivePuzzleMontage.Get());
        }
    }
    PuzzleShieldMesh->SetVisibility(false, true);
    PuzzleShieldMesh->SetHiddenInGame(true, true);
    bPuzzlePresentationActive = false;
    bPuzzleExitPending = false;
    ActivePuzzleMontage = nullptr;
    PuzzleAnimInstance.Reset();
    Super::EndPlay(EndPlayReason);
}

int32 ABossMonster::GetRemainingCoreCount() const
{
    return IsValid(CorePuzzleComponent.Get())
        ? CorePuzzleComponent->GetRemainingCoreCount() : 0;
}

void ABossMonster::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        StopLaserPuzzle();
    }
}

bool ABossMonster::CanStartLaserPuzzle() const
{
    if (!HasAuthority() || bLaserPuzzleActive
        || LaserEmitters.IsEmpty() || LaserReceivers.IsEmpty())
    {
        return false;
    }

    TSet<const AGimmickBase*> Seen;
    const auto ValidateActors = [&Seen](const auto& Actors)
        {
            for (const auto& Entry : Actors)
            {
                const AGimmickBase* Actor = Entry.Get();
                if (!IsValid(Actor) || Seen.Contains(Actor))
                {
                    return false;
                }
                Seen.Add(Actor);
            }
            return true;
        };

    if (!ValidateActors(LaserEmitters)
        || !ValidateActors(LaserReceivers)
        || !ValidateActors(LaserReflectors))
    {
        return false;
    }

    for (ALaserEmitter* Emitter : LaserEmitters)
    {
        if (!Emitter->LoadLaserConfig())
        {
            return false;
        }
    }
    return true;
}

bool ABossMonster::StartLaserPuzzle()
{
    if (CurrentState != EBossState::ShieldPuzzle
        || !IsValid(ShieldComponent.Get())
        || !ShieldComponent->IsShieldActive()
        || ShieldComponent->GetRemainingPuzzleTime() <= 0.f
        || !CanStartLaserPuzzle())
    {
        return false;
    }

    StopLaserPuzzle();
    bLaserPuzzleActive = true;
    SetActorTickEnabled(true);

    const auto EnableActor = [](AGimmickBase* Actor)
        {
            Actor->SetActorHiddenInGame(false);
            Actor->SetActorEnableCollision(true);
            Actor->SetGimmickEnabled(true);
        };

    for (ALaserReceiver* Receiver : LaserReceivers)
    {
        EnableActor(Receiver);
    }
    for (ALaserReflector* Reflector : LaserReflectors)
    {
        Reflector->ResetReflector();
        EnableActor(Reflector);
    }
    for (ALaserEmitter* Emitter : LaserEmitters)
    {
        if (!bLaserPuzzleActive || CurrentState != EBossState::ShieldPuzzle)
        {
            return false;
        }

        // 접촉 목록 갱신이 끝난 뒤 보스가 성공 여부를 검사한다.
        AddTickPrerequisiteActor(Emitter);
        EnableActor(Emitter);
        Emitter->ApplyActiveState(true);
    }

    return bLaserPuzzleActive && CurrentState == EBossState::ShieldPuzzle;
}

void ABossMonster::StopLaserPuzzle()
{
    bLaserPuzzleActive = false;

    const auto DisableActor = [](AGimmickBase* Actor)
        {
            if (!IsValid(Actor))
            {
                return;
            }

            Actor->SetGimmickEnabled(false);
            Actor->SetActorEnableCollision(false);
            Actor->SetActorHiddenInGame(true);
        };

    // 발신기를 먼저 꺼서 빔과 수신기 접촉을 즉시 정리한다.
    for (ALaserEmitter* Emitter : LaserEmitters)
    {
        DisableActor(Emitter);
    }
    for (ALaserReceiver* Receiver : LaserReceivers)
    {
        DisableActor(Receiver);
    }
    for (ALaserReflector* Reflector : LaserReflectors)
    {
        DisableActor(Reflector);
    }
}

void ABossMonster::CheckLaserPuzzleCompletion()
{
    if (!HasAuthority() || !bLaserPuzzleActive
        || CurrentState != EBossState::ShieldPuzzle
        || !IsValid(ShieldComponent.Get())
        || !ShieldComponent->IsShieldActive()
        || ShieldComponent->GetRemainingPuzzleTime() <= 0.f)
    {
        return;
    }

    for (ALaserReceiver* Receiver : LaserReceivers)
    {
        if (!IsValid(Receiver))
        {
            continue;
        }

        for (ALaserEmitter* Emitter : LaserEmitters)
        {
            if (Receiver->HasLaserContactFrom(Emitter))
            {
                CompleteShieldPuzzle();
                return;
            }
        }
    }
}

void ABossMonster::RemoveBossUI()
{
    if (IsValid(BossUIWidget.Get()))
    {
        BossUIWidget->SetVisibility(ESlateVisibility::Collapsed);
        BossUIWidget->RemoveFromParent();
    }
    BossUIWidget = nullptr;
}

void ABossMonster::SetBossUIVisible(bool bVisible)
{
    bBossUIVisible = bVisible;
    UpdateBossUI();
}

void ABossMonster::UpdateBossUI()
{
    if (!bBossUIVisible)
    {
        if (IsValid(BossUIWidget.Get()))
        {
            BossUIWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    APawn* TargetPawn = Cast<APawn>(GetCurrentTarget());
    APlayerController* PC = IsValid(TargetPawn)
        ? Cast<APlayerController>(TargetPawn->GetController()) : nullptr;

    if (CurrentState == EBossState::Dead
        || !IsValid(TargetPawn) || !IsValid(PC)
        || !PC->IsLocalController() || !PC->GetLocalPlayer()
        || PC->GetPawn() != TargetPawn || !BossUIWidgetClass)
    {
        RemoveBossUI();
        return;
    }

    if (IsValid(BossUIWidget.Get())
        && BossUIWidget->GetOwningPlayer() != PC)
    {
        RemoveBossUI();
    }

    if (!IsValid(BossUIWidget.Get()))
    {
        BossUIWidget = CreateWidget<UBossUIWidget>(PC, BossUIWidgetClass);
        if (!IsValid(BossUIWidget.Get()))
        {
            return;
        }
    }

    if (!BossUIWidget->IsInViewport())
    {
        if (!BossUIWidget->AddToPlayerScreen(30))
        {
            RemoveBossUI();
            return;
        }
    }

    // Widget 초기화 콜백에서 타깃이나 보스 상태가 변경된 경우도 처리한다.
    if (!IsValid(BossUIWidget.Get())
        || CurrentState == EBossState::Dead
        || GetCurrentTarget() != TargetPawn)
    {
        RemoveBossUI();
        return;
    }

    const float Health = IsValid(AttributeSet.Get())
        ? AttributeSet->GetHealth() : 0.f;
    const float MaxHealth = IsValid(AttributeSet.Get())
        ? AttributeSet->GetMaxHealth() : 0.f;
    const float HealthRatio = FMath::IsFinite(Health)
        && FMath::IsFinite(MaxHealth) && MaxHealth > 0.f
        ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;

    bool bShowPuzzle = false;
    float PuzzleRatio = 0.f;
    if (CurrentState == EBossState::ShieldPuzzle
        && IsValid(ShieldComponent.Get())
        && ShieldComponent->IsShieldActive())
    {
        const float TimeLimit = ShieldComponent->GetPuzzleTimeLimit();
        const float Remaining = ShieldComponent->GetRemainingPuzzleTime();
        bShowPuzzle = FMath::IsFinite(TimeLimit) && TimeLimit > 0.f
            && FMath::IsFinite(Remaining) && Remaining > 0.f;
        if (bShowPuzzle)
        {
            PuzzleRatio = FMath::Clamp(Remaining / TimeLimit, 0.f, 1.f);
        }
    }

    BossUIWidget->SetBossStatus(HealthRatio, bShowPuzzle, PuzzleRatio);
    BossUIWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ABossMonster::HandleBossHealthChanged(
    const FOnAttributeChangeData& Data)
{
    if (CurrentState == EBossState::Dead
        || !FMath::IsFinite(Data.OldValue)
        || !FMath::IsFinite(Data.NewValue)
        || Data.NewValue >= Data.OldValue)
    {
        return;
    }

    if (IsValid(HitEffect.Get()) && GetWorld())
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            HitEffect.Get(),
            GetActorLocation(),
            GetActorRotation());
    }

    if (Data.NewValue > 0.f)
    {
        StartHitFlash();
    }
}

void ABossMonster::StartHitFlash()
{
    if (CurrentState == EBossState::Dead
        || !IsValid(HitFlashMaterial.Get()) || !IsValid(GetMesh())
        || !GetWorld() || !FMath::IsFinite(HitFlashDuration)
        || HitFlashDuration <= 0.f)
    {
        return;
    }

    if (!bHitFlashActive)
    {
        PreviousHitOverlay = GetMesh()->GetOverlayMaterial();
        bHitFlashActive = true;
    }

    GetMesh()->SetOverlayMaterial(HitFlashMaterial.Get());
    GetWorldTimerManager().SetTimer(
        HitFlashTimerHandle, this, &ABossMonster::EndHitFlash,
        HitFlashDuration, false);
}

void ABossMonster::EndHitFlash()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(HitFlashTimerHandle);
    }

    if (bHitFlashActive && IsValid(GetMesh())
        && GetMesh()->GetOverlayMaterial() == HitFlashMaterial.Get())
    {
        GetMesh()->SetOverlayMaterial(PreviousHitOverlay.Get());
    }

    PreviousHitOverlay = nullptr;
    bHitFlashActive = false;
}

void ABossMonster::PlayDeathMontage()
{
    if (CurrentState != EBossState::Dead || IsActorBeingDestroyed())
    {
        return;
    }

    USkeletalMeshComponent* BossMesh = GetMesh();
    UAnimInstance* Anim = IsValid(BossMesh) ? BossMesh->GetAnimInstance() : nullptr;
    if (!IsValid(Anim) || !IsValid(DeathMontage.Get()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("%s: DeathMontage or AnimInstance is missing."), *GetName());
        return;
    }

    BossMesh->bPauseAnims = false;
    BossMesh->SetComponentTickEnabled(true);
    BossMesh->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    Anim->StopAllMontages(0.f);
    if (Anim->Montage_Play(DeathMontage.Get(), 1.f) <= 0.f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("%s: failed to play DeathMontage."), *GetName());
        return;
    }

    FAnimMontageInstance* Instance =
        Anim->GetActiveInstanceForMontage(DeathMontage.Get());
    if (!Instance)
    {
        Anim->Montage_Stop(0.f, DeathMontage.Get());
        UE_LOG(LogTemp, Warning,
            TEXT("%s: DeathMontage instance is missing."), *GetName());
        return;
    }

    // 재생 종료 위치에서 블렌드 아웃하지 않고 마지막 자세를 유지한다.
    Instance->bEnableAutoBlendOut = false;

    // 섹션이 여러 개여도 순서대로 한 번만 재생하고 마지막 섹션에서 멈춘다.
    const int32 SectionCount = DeathMontage->CompositeSections.Num();
    for (int32 Index = 0; Index < SectionCount; ++Index)
    {
        const FName NextSection = Index + 1 < SectionCount
            ? DeathMontage->GetSectionName(Index + 1) : NAME_None;
        Anim->Montage_SetNextSection(
            DeathMontage->GetSectionName(Index), NextSection, DeathMontage.Get());
    }
}


void ABossMonster::MulticastGunBurstMuzzle_Implementation(
    UNiagaraSystem* Effect, FName SocketName)
{
    if (GetNetMode() == NM_DedicatedServer || !IsValid(Effect)
        || !IsValid(GunMesh.Get()) || !GunMesh->DoesSocketExist(SocketName))
    {
        return;
    }

    UNiagaraFunctionLibrary::SpawnSystemAttached(
        Effect, GunMesh.Get(), SocketName,
        FVector::ZeroVector, FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget, true);
}
