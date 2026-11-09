#include "Character/Boss/GA_BossPullAttack.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UGA_BossPullAttack::UGA_BossPullAttack()
    : FTickableGameObject(ETickableTickType::Never)
{
    AttackType = EBossAttackType::PullAttack;
}

bool UGA_BossPullAttack::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    return IsValid(Boss) && Boss->GetBossPhase() == EBossPhase::Phase3
        && IsConfigValid()
        && Super::CanActivateAbility(
            Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UGA_BossPullAttack::IsConfigValid() const
{
    const auto Positive = [](float Value)
        {
            return FMath::IsFinite(Value) && Value > 0.f;
        };
    const UGameplayEffect* Effect = DamageEffectClass.GetDefaultObject();
    return IsValid(AttackMontage.Get())
        && AttackMontage->GetSectionIndex(ReadySection) != INDEX_NONE
        && AttackMontage->GetSectionIndex(LoopSection) != INDEX_NONE
        && AttackMontage->GetSectionIndex(EndSection) != INDEX_NONE
        && ReadySection != LoopSection && ReadySection != EndSection
        && LoopSection != EndSection
        && Positive(PullDuration) && Positive(PullRange)
        && Positive(DamageRange) && DamageRange < PullRange
        && Positive(RangeHalfHeight) && Positive(DamageInterval)
        && Positive(EffectBaseRadius) && Positive(RangeFadeInDuration)
        && FMath::IsFinite(PullStrength) && PullStrength >= 0.f
        && FMath::IsFinite(DamagePerTick) && DamagePerTick >= 0.f
        && !CenterOffset.ContainsNaN() && !ScaleParameter.IsNone()
        && !AlphaParameter.IsNone()
        && IsValid(RangeEffect.Get()) && IsValid(DamageAreaEffect.Get())
        && Effect && Effect->DurationPolicy == EGameplayEffectDurationType::Instant;
}

void UGA_BossPullAttack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || Boss->GetBossPhase() != EBossPhase::Phase3
        || !IsValid(Anim) || !IsConfigValid()
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bPullStarted = false;
    bPullActive = false;
    bEndRequested = false;
    MontageInstanceID = INDEX_NONE;
    BoundAnimInstance = Anim;
    Boss->OnBossStateChanged.AddUniqueDynamic(
        this, &UGA_BossPullAttack::HandleBossStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive() || bEnding)
    {
        return;
    }
    if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
    {
        AI->StopMovement();
    }
    Boss->GetCharacterMovement()->StopMovementImmediately();

    Anim->OnMontageSectionChanged.AddUniqueDynamic(
        this, &UGA_BossPullAttack::HandleSectionChanged);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage.Get(), 1.f, ReadySection,
        true, 1.f, 0.f, true);
    if (!IsValid(MontageTask.Get()))
    {
        HandleCancelled();
        return;
    }
    MontageTask->OnCompleted.AddDynamic(this, &UGA_BossPullAttack::HandleCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossPullAttack::HandleCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_BossPullAttack::HandleCancelled);
    RangeEffectStartTime = GetWorld()->GetTimeSeconds();
    SpawnEffect(RangeEffect.Get(), PullRange, RangeComponent, 0.f);

    MontageTask->ReadyForActivation();
    if (!IsActive() || bEnding)
    {
        return;
    }

    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(AttackMontage.Get());
    if (!Instance)
    {
        HandleCancelled();
        return;
    }
    MontageInstanceID = Instance->GetInstanceID();
    Anim->Montage_SetNextSection(ReadySection, LoopSection, AttackMontage.Get());
    Anim->Montage_SetNextSection(LoopSection, LoopSection, AttackMontage.Get());
    Anim->Montage_SetNextSection(EndSection, NAME_None, AttackMontage.Get());
    if (PatternStartSound)
    {
        Boss->MulticastGunBurstSound(PatternStartSound.Get(), Boss->GetActorLocation());
    }
    SetTickableTickType(ETickableTickType::Conditional);
    if (Anim->Montage_GetCurrentSection(AttackMontage.Get()) == LoopSection)
    {
        StartPull();
    }
}

bool UGA_BossPullAttack::IsContextValid() const
{
    ABossMonster* Boss = GetBossOwner();
    return IsValid(Boss) && Boss->HasAuthority()
        && Boss->GetBossState() == EBossState::Attacking
        && Boss->GetBossPhase() == EBossPhase::Phase3;
}

bool UGA_BossPullAttack::IsTickable() const
{
    return !IsTemplate() && IsActive() && !bEnding;
}

TStatId UGA_BossPullAttack::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGA_BossPullAttack, STATGROUP_Tickables);
}

UWorld* UGA_BossPullAttack::GetTickableGameObjectWorld() const
{
    return GetWorld();
}

void UGA_BossPullAttack::BeginDestroy()
{
    SetTickableTickType(ETickableTickType::Never);
    DestroyEffects();
    Super::BeginDestroy();
}

void UGA_BossPullAttack::HandleSectionChanged(
    UAnimMontage* Montage, FName SectionName, bool bLooped)
{
    if (IsActive() && !bEnding && Montage == AttackMontage.Get()
        && SectionName == LoopSection && !bPullStarted)
    {
        UAnimInstance* Anim = BoundAnimInstance.Get();
        FAnimMontageInstance* Instance = Anim
            ? Anim->GetActiveInstanceForMontage(Montage) : nullptr;
        if (Instance && Instance->GetInstanceID() == MontageInstanceID)
        {
            StartPull();
        }
    }
}

void UGA_BossPullAttack::SpawnEffect(
    UNiagaraSystem* System, float Radius,
    TObjectPtr<UNiagaraComponent>& OutComponent, float InitialAlpha)
{
    ABossMonster* Boss = GetBossOwner();
    if (GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    OutComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        System, Boss->GetRootComponent(), NAME_None,
        CenterOffset, FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset, false, false,
        ENCPoolMethod::None, false);
    if (IsValid(OutComponent.Get()))
    {
        // 활성화 전에 파라미터를 지정한다. 컴포넌트 크기는 중복 적용하지 않는다.
        OutComponent->SetAbsolute(false, false, true);
        OutComponent->SetWorldScale3D(FVector::OneVector);
        OutComponent->SetVariableFloat(ScaleParameter, Radius / EffectBaseRadius);
        OutComponent->SetVariableFloat(AlphaParameter, InitialAlpha);
        OutComponent->Activate(true);
    }
}

void UGA_BossPullAttack::StartPull()
{
    if (bPullStarted || !IsActive() || bEnding)
    {
        return;
    }
    if (!GetWorld() || !IsContextValid())
    {
        HandleCancelled();
        return;
    }
    bPullStarted = true;
    bPullActive = true;
    if (PullLoopEntrySound)
    {
        ABossMonster* Boss = GetBossOwner();
        Boss->MulticastGunBurstSound(PullLoopEntrySound.Get(), Boss->GetActorLocation());
    }
    const double Now = GetWorld()->GetTimeSeconds();
    PullEndTime = Now + PullDuration;
    NextDamageTime = Now;
    if (IsValid(RangeComponent.Get()))
    {
        RangeComponent->SetVariableFloat(AlphaParameter, 1.f);
    }
    SpawnEffect(DamageAreaEffect.Get(), DamageRange, DamageComponent);
}

void UGA_BossPullAttack::Tick(float DeltaTime)
{
    if (!IsTickable())
    {
        return;
    }
    if (!GetWorld() || !IsContextValid())
    {
        HandleCancelled();
        return;
    }
    // 종료 섹션의 정상 블렌드아웃은 몽타주 태스크의 완료 콜백을 기다린다.
    if (bEndRequested)
    {
        return;
    }
    UAnimInstance* Anim = BoundAnimInstance.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (!Instance || Instance->GetInstanceID() != MontageInstanceID)
    {
        HandleCancelled();
        return;
    }
    const FName Section = Anim->Montage_GetCurrentSection(AttackMontage.Get());
    if (!bPullStarted && Section == LoopSection)
    {
        StartPull();
    }
    if (!bPullStarted && IsValid(RangeComponent.Get()))
    {
        const double Elapsed = FMath::Max(
            0.0, GetWorld()->GetTimeSeconds() - RangeEffectStartTime);
        const float Progress = static_cast<float>(
            FMath::Clamp(Elapsed / RangeFadeInDuration, 0.0, 1.0));
        RangeComponent->SetVariableFloat(AlphaParameter, 0.15f * Progress);
    }

    if (!bPullActive || !IsActive() || bEnding)
    {
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= PullEndTime)
    {
        StopPull();
        return;
    }
    if (Section != LoopSection)
    {
        HandleCancelled();
        return;
    }
    const bool bDamageDue = Now >= NextDamageTime;
    if (bDamageDue)
    {
        // 프레임 지연 시 누락된 횟수를 한 프레임에 몰아서 타격하지 않는다.
        NextDamageTime += (FMath::FloorToDouble(
            (Now - NextDamageTime) / DamageInterval) + 1.0) * DamageInterval;
    }
    ABossMonster* Boss = GetBossOwner();
    const FVector Center = Boss->GetActorTransform().TransformPosition(CenterOffset);
    TArray<TWeakObjectPtr<APlayerBase>> Players;
    for (TActorIterator<APlayerBase> It(GetWorld()); It; ++It)
    {
        Players.Add(*It);
    }
    for (const TWeakObjectPtr<APlayerBase>& Entry : Players)
    {
        if (!IsActive() || bEnding || !bPullActive || !IsContextValid())
        {
            return;
        }
        APlayerBase* Player = Entry.Get();
        if (!IsValid(Player) || Player->IsDead() || !Player->IsPlayerControlled())
        {
            continue;
        }
        const FVector Position = Player->GetActorLocation();
        if (Position.ContainsNaN() || FMath::Abs(Position.Z - Center.Z) > RangeHalfHeight)
        {
            continue;
        }
        const double DistanceSquared = FVector::DistSquared2D(Position, Center);
        if (DistanceSquared > FMath::Square(PullRange))
        {
            continue;
        }
        if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
        {
            const FVector Direction = (Center - Position).GetSafeNormal2D();
            Movement->AddForce(Direction * PullStrength * Movement->Mass);
        }
        if (bDamageDue && DistanceSquared <= FMath::Square(DamageRange))
        {
            ApplyPullDamage(Player);
        }
    }
}

void UGA_BossPullAttack::ApplyPullDamage(APlayerBase* Player)
{
    ABossMonster* Boss = GetBossOwner();
    if (!IsValid(Boss) || !Boss->HasAuthority() || !IsValid(Player)
        || Player->IsDead() || DamagePerTick <= 0.f)
    {
        return;
    }
    UAbilitySystemComponent* SourceASC = Boss->GetAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent();
    if (!IsValid(SourceASC) || !IsValid(TargetASC)
        || !TargetASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetIncomingDamageAttribute()))
    {
        return;
    }
    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(Boss);
    Context.AddOrigin(Boss->GetActorTransform().TransformPosition(CenterOffset));
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
        DamageEffectClass, GetAbilityLevel(), Context);
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(WeaponGameplayTags::Data_Damage, DamagePerTick);
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    }
}

void UGA_BossPullAttack::DestroyEffects()
{
    if (IsValid(RangeComponent.Get()))
    {
        RangeComponent->DestroyComponent();
    }
    if (IsValid(DamageComponent.Get()))
    {
        DamageComponent->DestroyComponent();
    }
    RangeComponent = nullptr;
    DamageComponent = nullptr;
}

void UGA_BossPullAttack::StopPull()
{
    bPullActive = false;
    bEndRequested = true;
    DestroyEffects();
    if (UAnimInstance* Anim = BoundAnimInstance.Get())
    {
        Anim->Montage_JumpToSection(EndSection, AttackMontage.Get());
    }
    else
    {
        HandleCancelled();
    }
}

void UGA_BossPullAttack::HandleCompleted()
{
    if (IsActive() && !bEnding)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, !(bPullStarted && bEndRequested));
    }
}

void UGA_BossPullAttack::HandleCancelled()
{
    if (IsActive() && !bEnding)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossPullAttack::HandleBossStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Attacking)
    {
        HandleCancelled();
    }
}

void UGA_BossPullAttack::EndAbility(
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
    SetTickableTickType(ETickableTickType::Never);
    bPullActive = false;
    DestroyEffects();
    if (UAnimInstance* Anim = BoundAnimInstance.Get())
    {
        Anim->OnMontageSectionChanged.RemoveDynamic(
            this, &UGA_BossPullAttack::HandleSectionChanged);
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
            this, &UGA_BossPullAttack::HandleBossStateChanged);
        if (Boss->HasAuthority() && Boss->GetBossState() == EBossState::Attacking)
        {
            Boss->SetBossState(EBossState::Combat);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}