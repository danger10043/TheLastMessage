#include "Character/Boss/BossPatternComponent.h"
#include "Character/Boss/BossMonster.h"
#include "AbilitySystemComponent.h"

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