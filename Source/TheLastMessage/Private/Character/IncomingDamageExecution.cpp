#include "Character/IncomingDamageExecution.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttributeSet.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

void UIncomingDamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    UAbilitySystemComponent* TargetASC =
        ExecutionParams.GetTargetAbilitySystemComponent();
    if (!IsValid(TargetASC))
    {
        return;
    }

    AActor* Avatar = TargetASC->GetAvatarActor();
    if (!IsValid(Avatar) || !Avatar->HasAuthority())
    {
        return;
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    FGameplayTag Category;
    if (!UCombatDamageLibrary::TryGetDamageCategory(Spec, Category))
    {
        return;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Data.Damage")), false);
    if (!DamageTag.IsValid())
    {
        return;
    }
    const float Damage = Spec.GetSetByCallerMagnitude(DamageTag, false, -1.f);
    if (!FMath::IsFinite(Damage) || Damage <= 0.f)
    {
        return;
    }

    const bool bHasBossSet = TargetASC->GetSet<UBossAttributeSet>() != nullptr;
    const bool bHasEnemySet = TargetASC->GetSet<UEnemyAttributeSet>() != nullptr;
    if (bHasBossSet == bHasEnemySet)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CombatDamage: target must have exactly one Boss/Enemy AttributeSet."));
        return;
    }

    FGameplayAttribute IncomingAttribute;
    if (ABossMonster* Boss = Cast<ABossMonster>(Avatar))
    {
        if (!bHasBossSet || Boss->GetBossState() == EBossState::Dead)
        {
            return;
        }
        IncomingAttribute = UBossAttributeSet::GetIncomingDamageAttribute();
    }
    else if (AEnemyBase* Enemy = Cast<AEnemyBase>(Avatar))
    {
        if (!bHasEnemySet || Enemy->IsDead())
        {
            return;
        }
        IncomingAttribute = UEnemyAttributeSet::GetIncomingDamageAttribute();
    }
    else
    {
        return;
    }

    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        IncomingAttribute, EGameplayModOp::Additive, Damage));
}