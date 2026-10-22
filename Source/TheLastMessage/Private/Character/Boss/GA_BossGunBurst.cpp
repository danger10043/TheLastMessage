#include "Character/Boss/GA_BossGunBurst.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossBullet.h"
#include "Character/Player/PlayerBase.h"
#include "Framework/ActorPoolSubsystem.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

namespace
{
    const FName ReadySection(TEXT("GunReady"));
    const FName FireSection(TEXT("GunFire"));
    const FName EndSection(TEXT("GunEnd"));
    const FName ShotNotify(TEXT("GunShot"));
    const FName FireFinishedNotify(TEXT("GunFireFinished"));
}

UGA_BossGunBurst::UGA_BossGunBurst()
{
    AttackType = EBossAttackType::GunBurst;
    Phase2.Damage = 9.f;
    Phase2.ShotCount = 6;
    Phase2.ShotInterval = 1.f;
}

bool UGA_BossGunBurst::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    ABossMonster* Boss = ActorInfo
        ? Cast<ABossMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
    return IsValid(Boss)
        && (Boss->GetBossPhase() == EBossPhase::Phase1
            || Boss->GetBossPhase() == EBossPhase::Phase2)
        && Super::CanActivateAbility(
            Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_BossGunBurst::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || !IsValid(Player) || Player->IsDead() || !Player->IsPlayerControlled()
        || (Boss->GetBossPhase() != EBossPhase::Phase1
            && Boss->GetBossPhase() != EBossPhase::Phase2))
    {
        HandleCancelled();
        return;
    }

    LockedPhase = Boss->GetBossPhase();
    ActiveConfig = LockedPhase == EBossPhase::Phase1 ? Phase1 : Phase2;
    UStaticMeshComponent* Gun = Boss->GetGunMesh();
    const bool bValidConfig = BulletClass
        && FMath::IsFinite(ActiveConfig.Damage) && ActiveConfig.Damage >= 0.f
        && ActiveConfig.ShotCount > 0
        && FMath::IsFinite(ActiveConfig.ShotInterval) && ActiveConfig.ShotInterval > 0.f
        && FMath::IsFinite(BulletSpeed) && BulletSpeed > 0.f
        && FMath::IsFinite(BulletLifetime) && BulletLifetime > 0.f
        && !AimOriginOffset.ContainsNaN();

    if (!bValidConfig || !IsValid(Anim) || !AttackMontage
        || AttackMontage->HasRootMotion()
        || !FMath::IsNearlyEqual(AttackMontage->RateScale, 1.f) || !IsValid(Gun)
        || !Gun->DoesSocketExist(MuzzleSocketName))
    {
        UE_LOG(LogTemp, Error, TEXT("GunBurst: invalid config, montage, or muzzle socket."));
        HandleCancelled();
        return;
    }

    const int32 ReadyIndex = AttackMontage->GetSectionIndex(ReadySection);
    const int32 FireIndex = AttackMontage->GetSectionIndex(FireSection);
    const int32 EndIndex = AttackMontage->GetSectionIndex(EndSection);
    if (ReadyIndex == INDEX_NONE || FireIndex == INDEX_NONE || EndIndex == INDEX_NONE
        || AttackMontage->GetSectionLength(FireIndex) >= ActiveConfig.ShotInterval)
    {
        UE_LOG(LogTemp, Error, TEXT("GunBurst: check sections; GunFire must be shorter than ShotInterval."));
        HandleCancelled();
        return;
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        HandleCancelled();
        return;
    }

    Target = Player;
    ShotsFired = 0;
    bAwaitingShot = true;
    bFireClipFinished = false;
    MontageInstanceID = INDEX_NONE;
    BoundAnim = Anim;

    bSavedControllerYaw = Boss->bUseControllerRotationYaw;
    if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
    {
        bSavedOrientToMovement = Movement->bOrientRotationToMovement;
        bSavedDesiredRotation = Movement->bUseControllerDesiredRotation;
        Movement->StopMovementImmediately();
        Movement->bOrientRotationToMovement = false;
        Movement->bUseControllerDesiredRotation = false;
    }
    bSavedRotation = true;
    Boss->bUseControllerRotationYaw = false;
    if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
    {
        AI->StopMovement();
        AI->ClearFocus(EAIFocusPriority::Gameplay);
    }

    // Also acts as a fail-safe if a required montage notify is missing.
    Deadline = GetWorld()->GetTimeSeconds()
        + AttackMontage->GetPlayLength()
        + ActiveConfig.ShotCount * ActiveConfig.ShotInterval + 2.0;
    Boss->bGunBurstAiming = true;
    Boss->OnBossStateChanged.AddUniqueDynamic(this, &UGA_BossGunBurst::HandleBossStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive())
    {
        return;
    }

    UpdatePattern();
    if (!IsActive())
    {
        return;
    }
    GetWorld()->GetTimerManager().SetTimer(
        UpdateTimer, this, &UGA_BossGunBurst::UpdatePattern, 0.01f, true);

    Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UGA_BossGunBurst::HandleNotify);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage.Get(), 1.f, ReadySection, true, 1.f, 0.f, true);
    if (!MontageTask)
    {
        HandleCancelled();
        return;
    }
    MontageTask->OnCompleted.AddDynamic(this, &UGA_BossGunBurst::HandleCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossGunBurst::HandleCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_BossGunBurst::HandleCancelled);
    MontageTask->ReadyForActivation();
    if (!IsActive())
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
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    ASC->CurrentMontageSetNextSectionName(ReadySection, FireSection);
    ASC->CurrentMontageSetNextSectionName(FireSection, EndSection);
    ASC->CurrentMontageSetNextSectionName(EndSection, NAME_None);
}

bool UGA_BossGunBurst::IsContextValid() const
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = Target.Get();
    return IsValid(Boss) && Boss->HasAuthority()
        && Boss->GetBossState() == EBossState::Attacking
        && Boss->GetBossPhase() == LockedPhase
        && IsValid(Player) && Player->IsPlayerControlled() && !Player->IsDead()
        && Boss->GetCurrentTarget() == Player;
}

void UGA_BossGunBurst::UpdatePattern()
{
    if (!IsActive() || bEnding)
    {
        return;
    }
    if (!IsContextValid() || !GetWorld() || GetWorld()->GetTimeSeconds() > Deadline)
    {
        HandleCancelled();
        return;
    }

    ABossMonster* Boss = GetBossOwner();
    const FVector Delta = Target->GetActorLocation()
        - (Boss->GetActorLocation() + AimOriginOffset);
    const float Elevation = FMath::RadiansToDegrees(
        FMath::Atan2(static_cast<float>(Delta.Z), static_cast<float>(FMath::Abs(Delta.X))));
    Boss->GunBurstAimAngle = FMath::Clamp(Elevation, -70.f, 70.f);
    const float BodyTurn = FMath::GetMappedRangeValueClamped(
        FVector2D(70.f, 90.f), FVector2D(0.f, 90.f), Elevation);
    const float TargetYaw = Delta.X >= 0.f ? BodyTurn : 180.f - BodyTurn;
    FRotator Rotation = Boss->GetActorRotation();
    Rotation.Yaw = TargetYaw;
    Boss->SetActorRotation(Rotation);

    if (bFireClipFinished && ShotsFired < ActiveConfig.ShotCount
        && GetWorld()->GetTimeSeconds() >= NextFireStartTime)
    {
        bFireClipFinished = false;
        bAwaitingShot = true;
        UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
        if (!ASC || ASC->GetAnimatingAbility() != this)
        {
            HandleCancelled();
            return;
        }
        ASC->CurrentMontageJumpToSection(FireSection);
        ASC->CurrentMontageSetPlayRate(1.f);
    }
}

void UGA_BossGunBurst::HandleNotify(
    FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
    if (!IsActive() || bEnding || !IsContextValid())
    {
        return;
    }
    ABossMonster* Boss = GetBossOwner();
    UAnimInstance* Anim = BoundAnim.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (!Instance || Payload.SkelMeshComponent != Boss->GetMesh()
        || Payload.MontageInstanceID != Instance->GetInstanceID()
        || (MontageInstanceID != INDEX_NONE && Payload.MontageInstanceID != MontageInstanceID)
        || Instance->GetCurrentSection() != FireSection)
    {
        return;
    }

    if (NotifyName == ShotNotify && bAwaitingShot)
    {
        bAwaitingShot = false;
        if (ShotsFired >= ActiveConfig.ShotCount || !FireBullet())
        {
            HandleCancelled();
            return;
        }
        if (!IsActive())
        {
            return;
        }
        ++ShotsFired;
        float SectionStart = 0.f;
        float SectionEnd = 0.f;
        AttackMontage->GetSectionStartAndEndTime(
            AttackMontage->GetSectionIndex(FireSection), SectionStart, SectionEnd);
        const float ShotOffset = FMath::Max(0.f, Instance->GetPosition() - SectionStart);
        NextFireStartTime = GetWorld()->GetTimeSeconds()
            + ActiveConfig.ShotInterval - ShotOffset;
    }
    else if (NotifyName == FireFinishedNotify && !bFireClipFinished)
    {
        if (bAwaitingShot)
        {
            HandleCancelled();
            return;
        }
        if (ShotsFired < ActiveConfig.ShotCount)
        {
            bFireClipFinished = true;
            GetAbilitySystemComponentFromActorInfo()->CurrentMontageSetPlayRate(0.5711f);
        }
        // The final GunFire continues naturally into GunEnd.
    }
}

bool UGA_BossGunBurst::FireBullet()
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = Target.Get();
    UWorld* World = GetWorld();
    UStaticMeshComponent* Gun = IsValid(Boss) ? Boss->GetGunMesh() : nullptr;
    UActorPoolSubsystem* Pool = World
        ? World->GetSubsystem<UActorPoolSubsystem>() : nullptr;
    if (!IsActive() || bEnding || !IsContextValid()
        || !Pool || !IsValid(Gun) || !IsValid(Player)
        || !Gun->DoesSocketExist(MuzzleSocketName))
    {
        return false;
    }

    const FVector Origin = Gun->GetSocketLocation(MuzzleSocketName);
    const FVector Direction = (Player->GetActorLocation() - Origin).GetSafeNormal();
    if (Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero())
    {
        return false;
    }

    ABossBullet* Bullet = Cast<ABossBullet>(Pool->AcquireActor(
        BulletClass, FTransform(Direction.Rotation(), Origin), Boss, Boss));
    if (!IsValid(Bullet))
    {
        return false;
    }
    if (!IsActive() || bEnding || !IsContextValid())
    {
        Bullet->ReturnToPool();
        return false;
    }
    if (!Bullet->StartBullet(ActiveConfig.Damage, BulletSpeed, BulletLifetime))
    {
        Bullet->ReturnToPool();
        return false;
    }

    if (IsValid(Boss) && MuzzleEffect)
    {
        Boss->MulticastGunBurstMuzzle(MuzzleEffect.Get(), MuzzleSocketName);
    }
    return true;
}

void UGA_BossGunBurst::HandleCompleted()
{
    if (!IsActive() || bEnding)
    {
        return;
    }
    if (ShotsFired != ActiveConfig.ShotCount || bAwaitingShot || !IsContextValid())
    {
        HandleCancelled();
        return;
    }
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, false);
}

void UGA_BossGunBurst::HandleCancelled()
{
    if (IsActive() && !bEnding)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossGunBurst::HandleBossStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Attacking)
    {
        HandleCancelled();
    }
}

void UGA_BossGunBurst::EndAbility(
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
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    }
    if (UAnimInstance* Anim = BoundAnim.Get())
    {
        Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UGA_BossGunBurst::HandleNotify);
    }
    BoundAnim.Reset();
    if (MontageTask)
    {
        MontageTask->OnCompleted.RemoveAll(this);
        MontageTask->OnInterrupted.RemoveAll(this);
        MontageTask->OnCancelled.RemoveAll(this);
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC && ASC->GetAnimatingAbility() == this)
    {
        ASC->CurrentMontageSetPlayRate(1.f);
        ASC->CurrentMontageStop(0.1f);
        ASC->ClearAnimatingAbility(this);
    }
    if (MontageTask)
    {
        MontageTask->EndTask();
        MontageTask = nullptr;
    }

    ABossMonster* Boss = GetBossOwner();
    if (IsValid(Boss))
    {
        Boss->OnBossStateChanged.RemoveDynamic(this, &UGA_BossGunBurst::HandleBossStateChanged);
        Boss->bGunBurstAiming = false;
        Boss->GunBurstAimAngle = 0.f;
        if (bSavedRotation)
        {
            // Leave a conventional left/right facing before restoring normal AI.
            FRotator Rotation = Boss->GetActorRotation();
            Rotation.Yaw = FMath::Cos(FMath::DegreesToRadians(Rotation.Yaw)) >= 0.f ? 0.f : 180.f;
            Boss->SetActorRotation(Rotation);
            if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
            {
                AI->SetControlRotation(Rotation);
            }
            Boss->bUseControllerRotationYaw = bSavedControllerYaw;
            if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
            {
                Movement->bOrientRotationToMovement = bSavedOrientToMovement;
                Movement->bUseControllerDesiredRotation = bSavedDesiredRotation;
            }
        }
        if (Boss->HasAuthority() && Boss->GetBossState() == EBossState::Attacking)
        {
            Boss->SetBossState(EBossState::Combat);
        }
    }
    bSavedRotation = false;
    Target.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}