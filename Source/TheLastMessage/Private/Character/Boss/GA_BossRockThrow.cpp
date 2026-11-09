#include "Character/Boss/GA_BossRockThrow.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossRockProjectile.h"
#include "Character/Player/PlayerBase.h"
#include "Framework/ActorPoolSubsystem.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_BossRockThrow::UGA_BossRockThrow()
{
    AttackType = EBossAttackType::RockThrow;
}

bool UGA_BossRockThrow::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    return IsValid(Boss) && Boss->GetBossPhase() == EBossPhase::Phase3
        && Super::CanActivateAbility(
            Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_BossRockThrow::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    bThrown = false;
    bOwnsBossState = false;
    MontageInstanceID = INDEX_NONE;

    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || Boss->GetBossPhase() != EBossPhase::Phase3
        || !IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead()
        || !IsValid(Anim) || !AttackMontage || !RockClass
        || AttackMontage->HasRootMotion()
        || !FMath::IsNearlyEqual(AttackMontage->RateScale, 1.f)
        || !FMath::IsFinite(AttackMontage->GetPlayLength())
        || AttackMontage->GetPlayLength() <= 0.f
        || !FMath::IsFinite(Damage) || Damage < 0.f
        || !FMath::IsFinite(RockSpeed) || RockSpeed <= 0.f
        || !FMath::IsFinite(RockLifetime) || RockLifetime <= 0.f
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        HandleCancelled();
        return;
    }

    Target = Player;
    BoundAnim = Anim;
    Deadline = GetWorld()->GetTimeSeconds() + AttackMontage->GetPlayLength() + 2.0;

    if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
    {
        AI->StopMovement();
    }
    if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }

    Boss->OnBossStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandleBossStateChanged);
    bOwnsBossState = true;
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive() || bEnding)
    {
        return;
    }

    Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::HandleNotify);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage.Get(), 1.f, NAME_None, true, 1.f, 0.f, true);
    if (!MontageTask)
    {
        HandleCancelled();
        return;
    }
    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleCancelled);
    MontageTask->ReadyForActivation();
    if (!IsActive() || bEnding)
    {
        return;
    }

    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(AttackMontage.Get());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!Instance || !ASC || ASC->GetAnimatingAbility() != this)
    {
        HandleCancelled();
        return;
    }
    MontageInstanceID = Instance->GetInstanceID();
    GetWorld()->GetTimerManager().SetTimer(
        ContextTimer, this, &ThisClass::CheckContext, 0.05f, true);
}

bool UGA_BossRockThrow::IsContextValid() const
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = Target.Get();
    return IsValid(Boss) && Boss->HasAuthority()
        && Boss->GetBossPhase() == EBossPhase::Phase3
        && Boss->GetBossState() == EBossState::Attacking
        && IsValid(Player) && Player->IsPlayerControlled() && !Player->IsDead()
        && Boss->GetCurrentTarget() == Player;
}

void UGA_BossRockThrow::CheckContext()
{
    if (!IsActive() || bEnding)
    {
        return;
    }
    if (!GetWorld() || !IsContextValid()
        || GetWorld()->GetTimeSeconds() > Deadline)
    {
        HandleCancelled();
    }
}

void UGA_BossRockThrow::HandleNotify(
    FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
    if (!IsActive() || bEnding || bThrown
        || (NotifyName != TEXT("RockThrowReady") && NotifyName != TEXT("RockThrow"))
        || !IsContextValid())
    {
        return;
    }
    ABossMonster* Boss = GetBossOwner();
    UAnimInstance* Anim = BoundAnim.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (!Instance || Payload.SkelMeshComponent != Boss->GetMesh()
        || Payload.MontageInstanceID != Instance->GetInstanceID()
        || (MontageInstanceID != INDEX_NONE
            && Payload.MontageInstanceID != MontageInstanceID))
    {
        return;
    }

    const FVector FeedbackLocation = Boss->GetActorLocation();
    FHitResult GroundHit;
    FCollisionObjectQueryParams GroundObjects;
    GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    GroundObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    const FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(BossRockThrowGround), false, Boss);
    const FVector TraceEnd = FeedbackLocation - FVector(0.f, 0.f, 3000.f);
    const bool bHasGround = GetWorld()->LineTraceSingleByObjectType(
        GroundHit, FeedbackLocation, TraceEnd, GroundObjects, GroundQuery);
    if (NotifyName == TEXT("RockThrowReady"))
    {
        if (ReadySound)
        {
            Boss->MulticastGunBurstSound(ReadySound.Get(), FeedbackLocation);
        }
        if (ReadyEffect && bHasGround)
        {
            Boss->MulticastGroundEffect(ReadyEffect.Get(), GroundHit.ImpactPoint);
        }
        return;
    }

    bThrown = true;
    if (!ThrowRock())
    {
        HandleCancelled();
        return;
    }

    if (ThrowSound)
    {
        Boss->MulticastGunBurstSound(ThrowSound.Get(), FeedbackLocation);
    }
    if (ThrowEffect && bHasGround)
    {
        Boss->MulticastGroundEffect(ThrowEffect.Get(), GroundHit.ImpactPoint);
    }
}

bool UGA_BossRockThrow::ThrowRock()
{
    if (!GetWorld() || !IsContextValid())
    {
        return false;
    }
    ABossMonster* Boss = GetBossOwner();
    const FVector Origin = Boss->GetActorLocation();
    const FVector PlayerLocation = Target->GetActorLocation();
    const FVector Direction = (PlayerLocation - Origin).GetSafeNormal();
    if (Origin.ContainsNaN() || PlayerLocation.ContainsNaN()
        || Direction.ContainsNaN() || Direction.IsNearlyZero())
    {
        return false;
    }

    UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>();
    ABossRockProjectile* Rock = Pool ? Cast<ABossRockProjectile>(
        Pool->AcquireActor(RockClass,
            FTransform(Direction.Rotation(), Origin), Boss, Boss)) : nullptr;
    if (!IsValid(Rock))
    {
        return false;
    }
    if (!IsActive() || bEnding || !IsContextValid())
    {
        Rock->ReturnToPool();
        return false;
    }
    if (!Rock->StartBullet(Damage, RockSpeed, RockLifetime,
        RockFlightLoopSound.Get(), RockImpactSound.Get()))
    {
        Rock->ReturnToPool();
        return false;
    }
    return true;
}

void UGA_BossRockThrow::HandleCompleted()
{
    if (!IsActive() || bEnding)
    {
        return;
    }
    if (!bThrown || !IsContextValid())
    {
        HandleCancelled();
        return;
    }
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, false);
}

void UGA_BossRockThrow::HandleCancelled()
{
    if (IsActive() && !bEnding)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossRockThrow::HandleBossStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Attacking)
    {
        HandleCancelled();
    }
}

void UGA_BossRockThrow::EndAbility(
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
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ContextTimer);
    }
    if (UAnimInstance* Anim = BoundAnim.Get())
    {
        Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::HandleNotify);
    }
    if (MontageTask)
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
    }
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageStop(0.1f);
        ASC->ClearAnimatingAbility(this);
    }
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }
    if (ABossMonster* Boss = GetBossOwner(); IsValid(Boss))
    {
        Boss->OnBossStateChanged.RemoveDynamic(this, &ThisClass::HandleBossStateChanged);
        if (bOwnsBossState && Boss->HasAuthority()
            && Boss->GetBossState() == EBossState::Attacking)
        {
            Boss->SetBossState(EBossState::Combat);
        }
    }
    bOwnsBossState = false;
    BoundAnim.Reset();
    Target.Reset();
    MontageInstanceID = INDEX_NONE;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}