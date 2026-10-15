#include "Character/Boss/BossPatternComponent.h"
#include "Character/Boss/BossMonster.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"

float UBossPatternComponent::GetAttackCooldownRemaining(
    EBossAttackType AttackType) const
{
    const FBossAttackConfig* Config = AttackConfigs.FindByPredicate(
        [AttackType](const FBossAttackConfig& Entry)
        {
            return Entry.AttackType == AttackType;
        });

    const UWorld* World = GetWorld();
    if (AttackType == EBossAttackType::None || !Config || !World
        || !FMath::IsFinite(Config->Cooldown) || Config->Cooldown < 0.f)
    {
        return -1.f;
    }

    const double Now = World->GetTimeSeconds();
    if (!FMath::IsFinite(Now))
    {
        return -1.f;
    }

    const double* EndTime = CooldownEndTimes.Find(AttackType);
    return EndTime
        ? static_cast<float>(FMath::Max(0.0, *EndTime - Now))
        : 0.f;
}

bool UBossPatternComponent::TryStartAttackCooldown(EBossAttackType AttackType)
{
    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss) || !Boss->HasAuthority()
        || GetAttackCooldownRemaining(AttackType) != 0.f)
    {
        return false;
    }

    const FBossAttackConfig* Config = AttackConfigs.FindByPredicate(
        [AttackType](const FBossAttackConfig& Entry)
        {
            return Entry.AttackType == AttackType;
        });
    const UWorld* World = GetWorld();
    if (!Config || !World)
    {
        return false;
    }

    const double Now = World->GetTimeSeconds();
    const double EndTime = Now + static_cast<double>(Config->Cooldown);
    if (!FMath::IsFinite(EndTime))
    {
        return false;
    }

    CooldownEndTimes.Add(AttackType, EndTime);
    UE_LOG(LogTemp, Log,
        TEXT("BossCooldown: Boss=%s Type=%d Cooldown=%.2f Start=%.2f"),
        *Boss->GetName(), static_cast<int32>(AttackType), Config->Cooldown, Now);
    return true;
}

bool UBossPatternComponent::IsDistanceInAttackRange(
    EBossAttackType AttackType, float TargetDistance) const
{
    if (AttackType == EBossAttackType::None
        || !FMath::IsFinite(TargetDistance) || TargetDistance < 0.f)
    {
        return false;
    }

    const FBossAttackConfig* Config = AttackConfigs.FindByPredicate(
        [AttackType](const FBossAttackConfig& Entry)
        {
            return Entry.AttackType == AttackType;
        });

    return Config
        && FMath::IsFinite(Config->MinRange)
        && FMath::IsFinite(Config->MaxRange)
        && Config->MinRange >= 0.f
        && Config->MaxRange > 0.f
        && Config->MaxRange >= Config->MinRange
        && TargetDistance >= Config->MinRange
        && TargetDistance <= Config->MaxRange;
}

void UBossPatternComponent::CancelCurrentAttack()
{
    CurrentAttackType = EBossAttackType::None;
    bIsAttackExecuting = false;

    ABossMonster* Boss = Cast<ABossMonster>(GetOwner());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPatternComponent::CancelCurrentAttack - 소유자가 보스가 아닙니다."));
        return;
    }
    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!IsValid(ASC))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossPatternComponent::CancelCurrentAttack - ASC가 없습니다."));
        return;
    }
    ASC->CancelAllAbilities();
}