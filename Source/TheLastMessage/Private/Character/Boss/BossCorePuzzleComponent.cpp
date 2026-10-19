#include "Character/Boss/BossCorePuzzleComponent.h"
#include "Character/Boss/BossActivationCore.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossCircularMissile.h"
#include "Character/Player/PlayerBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool UBossCorePuzzleComponent::CanStartCorePuzzle(
    const TArray<TObjectPtr<ABossActivationCore>>& Cores) const
{
    if (!GetWorld() || !IsValid(GetOwner()) || !GetOwner()->HasAuthority()
        || bIsPuzzleActive || Cores.IsEmpty()
        || !PuzzleMissileClass
        || !FMath::IsFinite(PuzzleMissileLaunchTime)
        || PuzzleMissileLaunchTime < 0.01f
        || !FMath::IsFinite(PuzzleMissileDamage)
        || PuzzleMissileDamage < 0.f
        || !FMath::IsFinite(PuzzleMissileDamageRadius)
        || PuzzleMissileDamageRadius < 1.f
        || PuzzleMissileSpawnOffset.ContainsNaN())
    {
        return false;
    }

    TSet<ABossActivationCore*> Seen;
    for (ABossActivationCore* Core : Cores)
    {
        if (!IsValid(Core) || Seen.Contains(Core)
            || !Core->CanActivateFor(this))
        {
            return false;
        }
        Seen.Add(Core);
    }
    return true;
}

bool UBossCorePuzzleComponent::StartCorePuzzle(
    const TArray<TObjectPtr<ABossActivationCore>>& Cores)
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss)
        || Boss->GetBossState() != EBossState::ShieldPuzzle
        || Boss->GetRemainingPuzzleTime() <= 0.f
        || !CanStartCorePuzzle(Cores))
    {
        return false;
    }

    PuzzleCores = Cores;
    BrokenCores.Reset();
    RemainingCoreCount = PuzzleCores.Num();
    bIsPuzzleActive = true;

    for (int32 Index = 0; Index < PuzzleCores.Num(); ++Index)
    {
        if (!PuzzleCores[Index]->InitializeCore(Index, this))
        {
            StopCorePuzzle(true);
            return false;
        }
    }

    StopPuzzleMissiles();
    InitialPuzzleTime = Boss->GetRemainingPuzzleTime();
    PuzzleMissileTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
        this, &UBossCorePuzzleComponent::SpawnNextPuzzleMissile);

    return true;
}

bool UBossCorePuzzleComponent::CanAcceptHit(
    const ABossActivationCore* Core) const
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || !bIsPuzzleActive || !IsValid(Core)
        || Boss->GetBossState() != EBossState::ShieldPuzzle
        || Boss->GetRemainingPuzzleTime() <= 0.f)
    {
        return false;
    }

    for (ABossActivationCore* Entry : PuzzleCores)
    {
        if (Entry == Core)
        {
            return !BrokenCores.Contains(Entry);
        }
    }
    return false;
}

void UBossCorePuzzleComponent::HandleCoreDeactivated(
    ABossActivationCore* Core)
{
    if (!CanAcceptHit(Core) || !Core->IsBroken())
    {
        return;
    }

    BrokenCores.Add(Core);
    RemainingCoreCount = PuzzleCores.Num() - BrokenCores.Num();

    if (CheckPuzzleCompletion())
    {
        if (ABossMonster* Boss = Cast<ABossMonster>(GetOwner()))
        {
            Boss->CompleteShieldPuzzle();
        }
    }
}

bool UBossCorePuzzleComponent::CheckPuzzleCompletion() const
{
    return bIsPuzzleActive
        && !PuzzleCores.IsEmpty()
        && RemainingCoreCount == 0;
}

int32 UBossCorePuzzleComponent::GetRemainingCoreCount() const
{
    return RemainingCoreCount;
}

void UBossCorePuzzleComponent::StopCorePuzzle(bool bImmediate)
{
    // 애니메이션 콜백보다 먼저 피격과 집계를 중단한다.
    bIsPuzzleActive = false;
    StopPuzzleMissiles();
    RemainingCoreCount = 0;
    BrokenCores.Reset();

    const TArray<TObjectPtr<ABossActivationCore>> Cores = PuzzleCores;
    for (ABossActivationCore* Core : Cores)
    {
        if (!IsValid(Core))
        {
            continue;
        }

        if (bImmediate)
        {
            Core->ResetCore();
        }
        else
        {
            // 파괴된 코어는 자체 파괴 연출과 페이드를 계속한다.
            Core->DeactivateCore();
        }
    }
}

void UBossCorePuzzleComponent::ResetCorePuzzle()
{
    StopCorePuzzle(false);
}

void UBossCorePuzzleComponent::EndCorePuzzle()
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    const bool bImmediate = !IsValid(Boss)
        || Boss->GetBossState() == EBossState::Dead;
    StopCorePuzzle(bImmediate);
}

void UBossCorePuzzleComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopCorePuzzle(true);
    Super::EndPlay(EndPlayReason);
}

void UBossCorePuzzleComponent::ScheduleNextPuzzleMissile()
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!GetWorld() || !IsValid(Boss) || !Boss->HasAuthority()
        || !bIsPuzzleActive
        || Boss->GetBossState() != EBossState::ShieldPuzzle
        || InitialPuzzleTime <= 0.f)
    {
        return;
    }

    const float Remaining = Boss->GetRemainingPuzzleTime();
    if (Remaining <= 0.f)
    {
        return;
    }

    const float Progress = FMath::Clamp(
        1.f - Remaining / InitialPuzzleTime, 0.f, 1.f);
    const float Interval = FMath::Lerp(1.f, 0.08f, Progress);

    GetWorld()->GetTimerManager().SetTimer(
        PuzzleMissileTimer, this,
        &UBossCorePuzzleComponent::SpawnNextPuzzleMissile,
        Interval, false);
}

void UBossCorePuzzleComponent::SpawnNextPuzzleMissile()
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!GetWorld() || !IsValid(Boss) || !Boss->HasAuthority()
        || !bIsPuzzleActive
        || Boss->GetBossState() != EBossState::ShieldPuzzle
        || Boss->GetRemainingPuzzleTime() <= 0.f)
    {
        StopPuzzleMissiles();
        return;
    }

    const uint32 Generation = PuzzleMissileGeneration;
    APlayerBase* Target = PuzzleMissileTarget.Get();
    if (!IsValid(Target) || Target->IsDead() || !Target->IsPlayerControlled())
    {
        PuzzleMissileStep = 0;
        Target = Cast<APlayerBase>(Boss->GetCurrentTarget());
        if (!IsValid(Target) || Target->IsDead() || !Target->IsPlayerControlled())
        {
            Target = Cast<APlayerBase>(
                UGameplayStatics::GetPlayerPawn(this, 0));
        }
        PuzzleMissileTarget = Target;
    }

    if (!IsValid(Target) || Target->IsDead() || !Target->IsPlayerControlled())
    {
        PuzzleMissileTarget.Reset();
        ScheduleNextPuzzleMissile();
        return;
    }

    FVector FireLocation;
    if (PuzzleMissileStep == 0)
    {
        FireLocation = Target->GetActorLocation();
    }
    else if (PuzzleMissileStep == 1)
    {
        FireLocation = Target->GetActorLocation()
            + Target->GetVelocity() * PuzzleMissileLaunchTime;
    }
    else
    {
        FireLocation = (FirstMissileTarget + SecondMissileTarget) * 0.5;
    }

    if (FireLocation.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Boss core puzzle: invalid missile target location."));
        PuzzleMissileStep = 0;
        ScheduleNextPuzzleMissile();
        return;
    }

    const FVector SpawnLocation = Boss->GetActorLocation()
        + Boss->GetActorRotation().RotateVector(PuzzleMissileSpawnOffset);
    const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

    ABossCircularMissile* Missile =
        GetWorld()->SpawnActorDeferred<ABossCircularMissile>(
            PuzzleMissileClass, SpawnTransform, Boss, Boss,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Missile))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Boss core puzzle: failed to spawn a missile."));
        Boss->FailShieldPuzzle();
        return;
    }

    PuzzleMissiles.RemoveAll(
        [](const TWeakObjectPtr<ABossCircularMissile>& Entry)
        {
            return !Entry.IsValid();
        });
    PuzzleMissiles.Add(Missile);

    Missile->LaunchTime = PuzzleMissileLaunchTime;
    Missile->Damage = PuzzleMissileDamage;
    Missile->DamageRadius = PuzzleMissileDamageRadius;
    Missile->MissileLaunchDelta = 300.f;
    Missile->FireLocation = FireLocation;
    Missile->FinishSpawning(SpawnTransform);

    // BeginPlay 콜백 중 퍼즐이 종료되거나 새로 시작된 경우를 구분한다.
    if (Generation != PuzzleMissileGeneration)
    {
        if (IsValid(Missile))
        {
            Missile->Destroy();
        }
        return;
    }
    if (!IsValid(Missile))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Boss core puzzle: missile initialization failed."));
        Boss->FailShieldPuzzle();
        return;
    }
    if (!IsValid(Boss) || !bIsPuzzleActive
        || Boss->GetBossState() != EBossState::ShieldPuzzle)
    {
        StopPuzzleMissiles();
        return;
    }

    if (PuzzleMissileStep == 0)
    {
        FirstMissileTarget = FireLocation;
    }
    else if (PuzzleMissileStep == 1)
    {
        SecondMissileTarget = FireLocation;
    }

    PuzzleMissileStep = (PuzzleMissileStep + 1) % 3;
    ScheduleNextPuzzleMissile();
}

void UBossCorePuzzleComponent::StopPuzzleMissiles()
{
    ++PuzzleMissileGeneration;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PuzzleMissileTimer);
    }

    PuzzleMissileStep = 0;
    InitialPuzzleTime = 0.f;
    FirstMissileTarget = FVector::ZeroVector;
    SecondMissileTarget = FVector::ZeroVector;
    PuzzleMissileTarget.Reset();

    TArray<TWeakObjectPtr<ABossCircularMissile>> Missiles =
        MoveTemp(PuzzleMissiles);
    PuzzleMissiles.Reset();

    for (const TWeakObjectPtr<ABossCircularMissile>& Entry : Missiles)
    {
        if (ABossCircularMissile* Missile = Entry.Get())
        {
            Missile->Destroy();
        }
    }
}