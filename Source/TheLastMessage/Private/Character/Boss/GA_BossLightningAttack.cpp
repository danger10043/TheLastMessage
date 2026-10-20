#include "Character/Boss/GA_BossLightningAttack.h"
#include "Character/Boss/BossLightning.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "Framework/ActorPoolSubsystem.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UGA_BossLightningAttack::UGA_BossLightningAttack()
{
    AttackType = EBossAttackType::LightningAttack;
}

void UGA_BossLightningAttack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || !IsValid(Target) || Target->IsDead() || !Target->IsPlayerControlled()
        || !IsValid(Anim) || !IsValid(AttackMontage.Get()) || !LightningClass)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    LockedPhase = Boss->GetBossPhase();
    switch (LockedPhase)
    {
    case EBossPhase::Phase1: ActiveConfig = Phase1Config; break;
    case EBossPhase::Phase2: ActiveConfig = Phase2Config; break;
    case EBossPhase::Phase3: ActiveConfig = Phase3Config; break;
    default:
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (AttackMontage->GetSectionIndex(TEXT("LightningStart")) == INDEX_NONE
        || AttackMontage->GetSectionIndex(TEXT("LightningLoop")) == INDEX_NONE
        || AttackMontage->GetSectionIndex(TEXT("LightningEnd")) == INDEX_NONE
        || !FMath::IsFinite(ActiveConfig.Damage) || ActiveConfig.Damage < 0.f
        || !FMath::IsFinite(ActiveConfig.Radius) || ActiveConfig.Radius < 1.f
        || !FMath::IsFinite(ActiveConfig.WarningTime) || ActiveConfig.WarningTime < 0.f
        || !FMath::IsFinite(ActiveConfig.StormDuration) || ActiveConfig.StormDuration <= 0.f
        || !FMath::IsFinite(ActiveConfig.SpawnInterval) || ActiveConfig.SpawnInterval <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("BossLightningAttack: invalid config or montage sections."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    LockedTarget = Target;
    bStormStarted = false;
    bStormActive = false;
    bEndRequested = false;
    MontageInstanceID = INDEX_NONE;
    BoundAnimInstance = Anim;

    Boss->OnBossStateChanged.AddUniqueDynamic(
        this, &UGA_BossLightningAttack::HandleBossStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive() || bEnding)
    {
        return;
    }
    if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
    {
        AI->StopMovement();
    }
    if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    Anim->OnMontageSectionChanged.AddUniqueDynamic(
        this, &UGA_BossLightningAttack::HandleSectionChanged);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage.Get(), 1.f,
        TEXT("LightningStart"), true, 1.f, 0.f, true);
    if (!IsValid(MontageTask.Get()))
    {
        HandleMontageCancelled();
        return;
    }
    MontageTask->OnCompleted.AddDynamic(
        this, &UGA_BossLightningAttack::HandleMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(
        this, &UGA_BossLightningAttack::HandleMontageCancelled);
    MontageTask->OnCancelled.AddDynamic(
        this, &UGA_BossLightningAttack::HandleMontageCancelled);
    MontageTask->ReadyForActivation();
    if (!IsActive() || bEnding)
    {
        return;
    }

    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(AttackMontage.Get());
    if (!Instance)
    {
        HandleMontageCancelled();
        return;
    }
    MontageInstanceID = Instance->GetInstanceID();
    Anim->Montage_SetNextSection(TEXT("LightningStart"), TEXT("LightningLoop"), AttackMontage.Get());
    Anim->Montage_SetNextSection(TEXT("LightningLoop"), TEXT("LightningLoop"), AttackMontage.Get());
    Anim->Montage_SetNextSection(TEXT("LightningEnd"), NAME_None, AttackMontage.Get());

    GetWorld()->GetTimerManager().SetTimer(
        ContextTimer, this, &UGA_BossLightningAttack::CheckPatternContext, 0.05f, true);
    if (Anim->Montage_GetCurrentSection(AttackMontage.Get()) == TEXT("LightningLoop"))
    {
        StartStorm();
    }
}

bool UGA_BossLightningAttack::IsPatternContextValid() const
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = LockedTarget.Get();
    return IsValid(Boss) && Boss->HasAuthority()
        && Boss->GetBossState() == EBossState::Attacking
        && Boss->GetBossPhase() == LockedPhase
        && IsValid(Target) && !Target->IsDead() && Target->IsPlayerControlled()
        && Boss->GetCurrentTarget() == Target;
}

void UGA_BossLightningAttack::CheckPatternContext()
{
    if (IsActive() && !bEnding && !IsPatternContextValid())
    {
        HandleMontageCancelled();
    }
}

void UGA_BossLightningAttack::HandleSectionChanged(
    UAnimMontage* Montage, FName SectionName, bool bLooped)
{
    if (!IsActive() || bEnding || Montage != AttackMontage.Get()
        || SectionName != TEXT("LightningLoop") || bStormStarted)
    {
        return;
    }
    UAnimInstance* Anim = BoundAnimInstance.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (Instance && Instance->GetInstanceID() == MontageInstanceID
        && Anim->Montage_GetCurrentSection(AttackMontage.Get()) == TEXT("LightningLoop"))
    {
        StartStorm();
    }
}

void UGA_BossLightningAttack::StartStorm()
{
    if (!IsActive() || bEnding || bStormStarted)
    {
        return;
    }
    if (!GetWorld() || !IsPatternContextValid())
    {
        HandleMontageCancelled();
        return;
    }

    bStormStarted = true;
    bStormActive = true;

    StopStormEffect();
    ABossMonster* Boss = GetBossOwner();
    if (IsValid(StormEffect.Get()) && IsValid(Boss)
        && IsValid(Boss->GetRootComponent())
        && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        StormEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            StormEffect.Get(), Boss->GetRootComponent(), NAME_None,
            FVector(0.f, 0.f, -250.f), FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            false, true, ENCPoolMethod::None, false);
    }

    StormEndTime = GetWorld()->GetTimeSeconds() + ActiveConfig.StormDuration;
    GetWorld()->GetTimerManager().SetTimer(
        StormTimer, this, &UGA_BossLightningAttack::StopStorm,
        ActiveConfig.StormDuration, false);
    GetWorld()->GetTimerManager().SetTimer(
        SpawnTimer, this, &UGA_BossLightningAttack::SpawnLightning,
        ActiveConfig.SpawnInterval, true);
    UE_LOG(LogTemp, Log, TEXT("BossLightningAttack: storm started, duration=%.2f"),
        ActiveConfig.StormDuration);
    SpawnLightning();
}

void UGA_BossLightningAttack::SpawnLightning()
{
    if (!IsActive() || bEnding || !bStormActive)
    {
        return;
    }
    if (!GetWorld() || !IsPatternContextValid())
    {
        HandleMontageCancelled();
        return;
    }
    if (GetWorld()->GetTimeSeconds() >= StormEndTime)
    {
        StopStorm();
        return;
    }

    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = LockedTarget.Get();
    const FVector Location = Target->GetActorLocation();
    if (Location.ContainsNaN())
    {
        HandleMontageCancelled();
        return;
    }
    UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>();
    ABossLightning* Lightning = Pool ? Cast<ABossLightning>(
        Pool->AcquireActor(LightningClass,
            FTransform(FRotator::ZeroRotator, Location), Boss, Boss)) : nullptr;
    if (!IsValid(Lightning))
    {
        HandleMontageCancelled();
        return;
    }
    if (!IsActive() || bEnding || !bStormActive || !IsPatternContextValid())
    {
        Lightning->ReturnToPool();
        return;
    }

    const FString LightningName = Lightning->GetName();
    if (!Lightning->StartLightning(
        ActiveConfig.Damage, ActiveConfig.Radius, ActiveConfig.WarningTime))
    {
        HandleMontageCancelled();
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("BossLightningAttack: lightning=%s, target=%s"),
        *LightningName, *Location.ToString());
}

void UGA_BossLightningAttack::StopStorm()
{
    if (!IsActive() || bEnding || !bStormActive)
    {
        return;
    }
    bStormActive = false;
    bEndRequested = true;
    StopStormEffect();
    GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
    GetWorld()->GetTimerManager().ClearTimer(StormTimer);

    UAnimInstance* Anim = BoundAnimInstance.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (!Instance || Instance->GetInstanceID() != MontageInstanceID)
    {
        HandleMontageCancelled();
        return;
    }
    Anim->Montage_JumpToSection(TEXT("LightningEnd"), AttackMontage.Get());
    UE_LOG(LogTemp, Log, TEXT("BossLightningAttack: storm ended -> LightningEnd"));
}

void UGA_BossLightningAttack::HandleMontageCompleted()
{
    if (!IsActive() || bEnding)
    {
        return;
    }
    const bool bCompleted = bStormStarted && bEndRequested && !bStormActive;
    if (!bCompleted)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossLightningAttack: montage ended before storm completion."));
    }
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, !bCompleted);
}

void UGA_BossLightningAttack::HandleMontageCancelled()
{
    if (IsActive() && !bEnding)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossLightningAttack::HandleBossStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Attacking)
    {
        HandleMontageCancelled();
    }
}

void UGA_BossLightningAttack::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bEnding || !IsEndAbilityValid(Handle, ActorInfo))
    {
        return;
    }
    TGuardValue<bool> EndingGuard(bEnding, true);
    bStormActive = false;
    StopStormEffect();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SpawnTimer);
        World->GetTimerManager().ClearTimer(StormTimer);
        World->GetTimerManager().ClearTimer(ContextTimer);
    }
    if (UAnimInstance* Anim = BoundAnimInstance.Get())
    {
        Anim->OnMontageSectionChanged.RemoveDynamic(
            this, &UGA_BossLightningAttack::HandleSectionChanged);
    }
    BoundAnimInstance.Reset();
    MontageInstanceID = INDEX_NONE;
    if (IsValid(MontageTask.Get()))
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
    }
    MontageTask = nullptr;

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (IsValid(ASC) && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageStop(0.1f);
        ASC->ClearAnimatingAbility(this);
    }
    if (ABossMonster* Boss = GetBossOwner())
    {
        Boss->OnBossStateChanged.RemoveDynamic(
            this, &UGA_BossLightningAttack::HandleBossStateChanged);
        if (Boss->HasAuthority() && Boss->GetBossState() == EBossState::Attacking)
        {
            Boss->SetBossState(EBossState::Combat);
        }
    }
    LockedTarget.Reset();

    // 생성된 번개는 보관하지 않는다. 각 액터가 경고/타격 후 스스로 반환한다.
    Super::EndAbility(Handle, ActorInfo, ActivationInfo,
        bReplicateEndAbility, bWasCancelled);
}

void UGA_BossLightningAttack::StopStormEffect()
{
    if (IsValid(StormEffectComponent.Get()))
    {
        StormEffectComponent->DeactivateImmediate();
        StormEffectComponent->DestroyComponent();
    }
    StormEffectComponent = nullptr;
}