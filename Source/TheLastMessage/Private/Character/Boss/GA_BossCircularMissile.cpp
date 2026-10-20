#include "Character/Boss/GA_BossCircularMissile.h"
#include "Character/Boss/BossCircularMissile.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Player/PlayerBase.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Framework/ActorPoolSubsystem.h"

UGA_BossCircularMissile::UGA_BossCircularMissile()
{
    AttackType = EBossAttackType::CircularMissile;
}

void UGA_BossCircularMissile::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = IsValid(Boss)
        ? Cast<APlayerBase>(Boss->GetCurrentTarget()) : nullptr;

    if (!IsValid(Boss) || !Boss->HasAuthority() || !GetWorld()
        || !IsValid(Target) || !Target->IsPlayerControlled() || Target->IsDead())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    LockedPhase = Boss->GetBossPhase();
    ActiveConfig = Boss->GetMissileConfig(LockedPhase);
    if (!ActiveConfig.CircularMissileClass || ActiveConfig.MissileCount < 2
        || !FMath::IsFinite(ActiveConfig.Damage) || ActiveConfig.Damage < 0.f
        || !FMath::IsFinite(ActiveConfig.LaunchTime) || ActiveConfig.LaunchTime < 0.01f
        || !FMath::IsFinite(ActiveConfig.DamageRadius) || ActiveConfig.DamageRadius < 1.f
        || !FMath::IsFinite(ActiveConfig.SpawnInterval) || ActiveConfig.SpawnInterval <= 0.f
        || MissileSpawnOffset.ContainsNaN())
    {
        UE_LOG(LogTemp, Error, TEXT("BossCircularMissile: invalid phase config."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    TargetA = FVector::ZeroVector;
    TargetB = FVector::ZeroVector;

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    LockedTarget = Target;
    SpawnedMissiles.Reset();
    SpawnedCount = 0;

    Boss->OnBossStateChanged.AddUniqueDynamic(
        this, &UGA_BossCircularMissile::HandleBossStateChanged);
    Boss->SetBossState(EBossState::Attacking);
    if (!IsActive())
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

    GetWorld()->GetTimerManager().SetTimer(
        SpawnTimer, this, &UGA_BossCircularMissile::SpawnNextMissile,
        ActiveConfig.SpawnInterval, true);
    SpawnNextMissile();
}

void UGA_BossCircularMissile::SpawnNextMissile()
{
    if (!IsActive() || bEnding)
    {
        return;
    }

    ABossMonster* Boss = GetBossOwner();
    APlayerBase* Target = LockedTarget.Get();
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || Boss->GetBossState() != EBossState::Attacking
        || Boss->GetBossPhase() != LockedPhase
        || !IsValid(Target) || Target->IsDead() || !Target->IsPlayerControlled()
        || Boss->GetCurrentTarget() != Target)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    const bool bFirstInPair = (SpawnedCount % 2) == 0;
    if (bFirstInPair)
    {
        TargetA = Target->GetActorLocation();
        FVector Velocity = Target->GetVelocity();
        Velocity.Y = 0.f;
        TargetB = TargetA + Velocity * ActiveConfig.LaunchTime;
    }

    const FVector FireLocation = bFirstInPair ? TargetA : TargetB;
    if (FireLocation.ContainsNaN())
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    const FVector SpawnLocation = Boss->GetActorLocation()
        + Boss->GetActorRotation().RotateVector(MissileSpawnOffset);
    const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
    UActorPoolSubsystem* Pool =
        GetWorld()->GetSubsystem<UActorPoolSubsystem>();
    if (!Pool)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    ABossCircularMissile* Missile = Cast<ABossCircularMissile>(
        Pool->AcquireActor(
            ActiveConfig.CircularMissileClass, SpawnTransform, Boss, Boss));
    if (!IsValid(Missile))
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    if (!IsActive() || bEnding)
    {
        Missile->ReturnToPool();
        return;
    }

    const TWeakObjectPtr<ABossCircularMissile> WeakMissile(Missile);
    Missile->OnMissileReturned.BindWeakLambda(this, [this, WeakMissile]()
        {
            SpawnedMissiles.Remove(WeakMissile);
        });
    SpawnedMissiles.Add(WeakMissile);

    Missile->Damage = ActiveConfig.Damage;
    Missile->LaunchTime = ActiveConfig.LaunchTime;
    Missile->DamageRadius = ActiveConfig.DamageRadius;
    Missile->FireLocation = FireLocation;
    Missile->StartMissile();

    if (!IsActive() || bEnding)
    {
        if (IsValid(Missile))
        {
            Missile->ReturnToPool();
        }
        return;
    }
    if (!IsValid(Missile) || !Missile->IsMissileActive())
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossCircularMissile: missile initialization failed."));
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
        return;
    }

    ++SpawnedCount;
    UE_LOG(LogTemp, Log,
        TEXT("BossCircularMissile: shot %d/%d, missile=%s, target=%s"),
        SpawnedCount, ActiveConfig.MissileCount,
        *Missile->GetName(), *FireLocation.ToString());

    if (SpawnedCount >= ActiveConfig.MissileCount)
    {
        GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
        GetWorld()->GetTimerManager().SetTimer(
            FinishTimer, this, &UGA_BossCircularMissile::CheckMissilesFinished,
            0.05f, true, ActiveConfig.LaunchTime);
    }
}

void UGA_BossCircularMissile::CheckMissilesFinished()
{
    if (!IsActive() || bEnding)
    {
        return;
    }

    for (const TWeakObjectPtr<ABossCircularMissile>& Missile : SpawnedMissiles)
    {
        if (IsValid(Missile.Get()))
        {
            return;
        }
    }

    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
        GetCurrentActivationInfo(), true, false);
}

void UGA_BossCircularMissile::HandleBossStateChanged(EBossState NewState)
{
    if (IsActive() && !bEnding && NewState != EBossState::Attacking)
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
            GetCurrentActivationInfo(), true, true);
    }
}

void UGA_BossCircularMissile::EndAbility(
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
        World->GetTimerManager().ClearTimer(SpawnTimer);
        World->GetTimerManager().ClearTimer(FinishTimer);
    }

    ABossMonster* Boss = GetBossOwner();
    if (IsValid(Boss))
    {
        Boss->OnBossStateChanged.RemoveDynamic(
            this, &UGA_BossCircularMissile::HandleBossStateChanged);
    }

    TArray<TWeakObjectPtr<ABossCircularMissile>> Missiles =
        MoveTemp(SpawnedMissiles);
    SpawnedMissiles.Reset();

    if (bWasCancelled)
    {
        for (const TWeakObjectPtr<ABossCircularMissile>& Entry : Missiles)
        {
            if (ABossCircularMissile* Missile = Entry.Get())
            {
                Missile->ReturnToPool();
            }
        }
    }
    LockedTarget.Reset();

    if (IsValid(Boss) && Boss->HasAuthority()
        && Boss->GetBossState() == EBossState::Attacking)
    {
        Boss->SetBossState(EBossState::Combat);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo,
        bReplicateEndAbility, bWasCancelled);
}