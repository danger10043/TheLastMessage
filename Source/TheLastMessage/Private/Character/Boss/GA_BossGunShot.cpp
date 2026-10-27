#include "Character/Boss/GA_BossGunShot.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossBullet.h"
#include "Character/Player/PlayerBase.h"
#include "Framework/ActorPoolSubsystem.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

namespace
{
    const FName GunShotReadySection(TEXT("GunReady"));
    const FName GunShotFireSection(TEXT("GunFire"));
    const FName GunShotEndSection(TEXT("GunEnd"));
}

UGA_BossGunShot::UGA_BossGunShot()
{
    AttackType = EBossAttackType::GunShot;
    Phase2.Damage = 8.f;
    Phase2.BulletCount = 9;
    Phase2.AnglePerBullet = 10.f;
}

bool UGA_BossGunShot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

void UGA_BossGunShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;
    UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
    UStaticMeshComponent* Gun = IsValid(Boss) ? Boss->GetGunMesh() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || !IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDead()
        || (Boss->GetBossPhase() != EBossPhase::Phase1
            && Boss->GetBossPhase() != EBossPhase::Phase2))
    {
        HandleCancelled();
        return;
    }

    LockedPhase = Boss->GetBossPhase();
    ActiveConfig = LockedPhase == EBossPhase::Phase1 ? Phase1 : Phase2;
    if (!BulletClass || !IsValid(Anim) || !AttackMontage
        || AttackMontage->HasRootMotion()
        || !FMath::IsNearlyEqual(AttackMontage->RateScale, 1.f)
        || !IsValid(Gun) || !Gun->DoesSocketExist(MuzzleSocketName)
        || !FMath::IsFinite(ActiveConfig.Damage) || ActiveConfig.Damage < 0.f
        || ActiveConfig.BulletCount <= 0 || ActiveConfig.BulletCount % 2 == 0
        || !FMath::IsFinite(ActiveConfig.AnglePerBullet)
        || ActiveConfig.AnglePerBullet < 0.f
        || (ActiveConfig.BulletCount - 1) * ActiveConfig.AnglePerBullet > 180.f
        || !FMath::IsFinite(BulletSpeed) || BulletSpeed <= 0.f
        || !FMath::IsFinite(BulletLifetime) || BulletLifetime <= 0.f
        || AimOriginOffset.ContainsNaN()
        || AttackMontage->GetSectionIndex(GunShotReadySection) == INDEX_NONE
        || AttackMontage->GetSectionIndex(GunShotFireSection) == INDEX_NONE
        || AttackMontage->GetSectionIndex(GunShotEndSection) == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("GunShot: invalid phase config, montage, or muzzle."));
        HandleCancelled();
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        HandleCancelled();
        return;
    }

    Target = Player;
    BoundAnim = Anim;
    bFired = false;
    MontageInstanceID = INDEX_NONE;
    Deadline = GetWorld()->GetTimeSeconds() + AttackMontage->GetPlayLength() + 2.0;
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
    Boss->bGunBurstAiming = true;
    Boss->OnBossStateChanged.AddUniqueDynamic(this, &UGA_BossGunShot::HandleBossStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive()) return;
    UpdatePattern();
    if (!IsActive()) return;

    Anim->OnMontageSectionChanged.AddUniqueDynamic(this, &UGA_BossGunShot::HandleSectionChanged);
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttackMontage.Get(), 1.f, GunShotReadySection, true, 1.f, 0.f, true);
    if (!MontageTask)
    {
        HandleCancelled();
        return;
    }
    MontageTask->OnCompleted.AddDynamic(this, &UGA_BossGunShot::HandleCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_BossGunShot::HandleCancelled);
    MontageTask->OnCancelled.AddDynamic(this, &UGA_BossGunShot::HandleCancelled);
    MontageTask->ReadyForActivation();
    if (!IsActive()) return;

    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(AttackMontage.Get());
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!Instance || !ASC || ASC->GetAnimatingAbility() != this)
    {
        HandleCancelled();
        return;
    }
    MontageInstanceID = Instance->GetInstanceID();
    ASC->CurrentMontageSetNextSectionName(GunShotReadySection, GunShotFireSection);
    ASC->CurrentMontageSetNextSectionName(GunShotFireSection, GunShotEndSection);
    ASC->CurrentMontageSetNextSectionName(GunShotEndSection, NAME_None);
    GetWorld()->GetTimerManager().SetTimer(
        UpdateTimer, this, &UGA_BossGunShot::UpdatePattern, 0.01f, true);
    HandleSectionChanged(AttackMontage.Get(), Instance->GetCurrentSection(), false);
}

bool UGA_BossGunShot::IsContextValid() const
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Player = Target.Get();
    return IsValid(Boss) && Boss->HasAuthority()
        && Boss->GetBossState() == EBossState::Attacking
        && Boss->GetBossPhase() == LockedPhase
        && IsValid(Player) && Player->IsPlayerControlled() && !Player->IsDead()
        && Boss->GetCurrentTarget() == Player;
}

void UGA_BossGunShot::UpdatePattern()
{
    if (!IsActive() || bEnding) return;
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
    FRotator Rotation = Boss->GetActorRotation();
    Rotation.Yaw = Delta.X >= 0.f ? BodyTurn : 180.f - BodyTurn;
    Boss->SetActorRotation(Rotation);
}

void UGA_BossGunShot::HandleSectionChanged(
    UAnimMontage* Montage, FName SectionName, bool bLooped)
{
    if (!IsActive() || bEnding || bFired || Montage != AttackMontage.Get()
        || SectionName != GunShotFireSection) return;
    UAnimInstance* Anim = BoundAnim.Get();
    FAnimMontageInstance* Instance = Anim
        ? Anim->GetActiveInstanceForMontage(AttackMontage.Get()) : nullptr;
    if (!Instance || Instance->GetInstanceID() != MontageInstanceID) return;
    bFired = true;
    if (!FireFan()) HandleCancelled();
}

bool UGA_BossGunShot::FireFan()
{
    if (!IsContextValid()) return false;
    ABossMonster* Boss = GetBossOwner();
    UStaticMeshComponent* Gun = Boss->GetGunMesh();
    UActorPoolSubsystem* Pool = GetWorld()->GetSubsystem<UActorPoolSubsystem>();
    if (!Pool || !IsValid(Gun) || !Gun->DoesSocketExist(MuzzleSocketName)) return false;

    const FVector Origin = Gun->GetSocketLocation(MuzzleSocketName);
    const FVector CenterDirection = (Target->GetActorLocation() - Origin).GetSafeNormal();
    if (Origin.ContainsNaN() || CenterDirection.ContainsNaN()
        || CenterDirection.IsNearlyZero()) return false;

    for (int32 Index = 0; Index < ActiveConfig.BulletCount; ++Index)
    {
        if (!IsActive() || bEnding || !IsContextValid()) return false;
        const float Angle = (Index - ActiveConfig.BulletCount / 2)
            * ActiveConfig.AnglePerBullet;
        const FVector Direction = CenterDirection.RotateAngleAxis(Angle, FVector::RightVector);
        ABossBullet* Bullet = Cast<ABossBullet>(Pool->AcquireActor(
            BulletClass, FTransform(Direction.Rotation(), Origin), Boss, Boss));
        if (!IsValid(Bullet)) return false;
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
    }
    return true;
}

void UGA_BossGunShot::HandleCompleted()
{
    if (!IsActive() || bEnding) return;
    if (!bFired || !IsContextValid())
    {
        HandleCancelled();
        return;
    }
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, false);
}

void UGA_BossGunShot::HandleCancelled()
{
    if (IsActive() && !bEnding)
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
}

void UGA_BossGunShot::HandleBossStateChanged(EBossState NewState)
{
    if (NewState != EBossState::Attacking) HandleCancelled();
}

void UGA_BossGunShot::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bEnding || !IsEndAbilityValid(Handle, ActorInfo)) return;
    TGuardValue<bool> EndingGuard(bEnding, true);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    if (UAnimInstance* Anim = BoundAnim.Get())
        Anim->OnMontageSectionChanged.RemoveDynamic(this, &UGA_BossGunShot::HandleSectionChanged);
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
        Boss->OnBossStateChanged.RemoveDynamic(this, &UGA_BossGunShot::HandleBossStateChanged);
        if (bSavedRotation)
        {
            Boss->bGunBurstAiming = false;
            Boss->GunBurstAimAngle = 0.f;
            FRotator Rotation = Boss->GetActorRotation();
            Rotation.Yaw = FMath::Cos(FMath::DegreesToRadians(Rotation.Yaw)) >= 0.f ? 0.f : 180.f;
            Boss->SetActorRotation(Rotation);
            if (AAIController* AI = Cast<AAIController>(Boss->GetController()))
                AI->SetControlRotation(Rotation);
            Boss->bUseControllerRotationYaw = bSavedControllerYaw;
            if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
            {
                Movement->bOrientRotationToMovement = bSavedOrientToMovement;
                Movement->bUseControllerDesiredRotation = bSavedDesiredRotation;
            }
            if (Boss->HasAuthority() && Boss->GetBossState() == EBossState::Attacking)
                Boss->SetBossState(EBossState::Combat);
        }
    }
    bSavedRotation = false;
    Target.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}