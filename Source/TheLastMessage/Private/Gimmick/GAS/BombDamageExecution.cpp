// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/GAS/BombDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

void UBombDamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

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

    FGameplayTag DamageCategory;

    if (!UCombatDamageLibrary::TryGetDamageCategory(Spec, DamageCategory) || DamageCategory.GetTagName() != FName(TEXT("Damage.Category.Gimmick")))
    {
        return;
    }

    APlayerBase* Player = Cast<APlayerBase>(Avatar);

    // 몬스터와 보스는 기존 계산 경로 사용
    if (!IsValid(Player))
    {
        Super::Execute_Implementation(ExecutionParams, OutExecutionOutput);

        return;
    }

    if (Player->IsDead() || !TargetASC->GetSet<UCombatAttributeSet>())
    {
        return;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage")), false);

    if (!DamageTag.IsValid())
    {
        return;
    }

    const float Damage = Spec.GetSetByCallerMagnitude(DamageTag, false, -1.0f);

    if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }

    // 받는 피해 배율과 사망 처리는 기존 AttributeSet에서 수행
    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            UCombatAttributeSet::GetIncomingDamageAttribute(),
            EGameplayModOp::Additive,
            Damage
        )
    );
}