#include "Character/Boss/BossPatternComponent.h"
#include "Character/Boss/BossMonster.h"
#include "AbilitySystemComponent.h"

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