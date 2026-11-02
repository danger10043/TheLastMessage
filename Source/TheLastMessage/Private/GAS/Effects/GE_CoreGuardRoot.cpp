#include "GAS/Effects/GE_CoreGuardRoot.h"
#include "GAS/CombatAttributeSet.h"

UGE_CoreGuardRoot::UGE_CoreGuardRoot()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 이동속도·충전 속도 코어의 배율과 관계없이 가드 중에는 0으로 덮어쓴다.
	for (const FGameplayAttribute& Attribute : {UCombatAttributeSet::GetMoveSpeedMultiplierAttribute(),
		UCombatAttributeSet::GetEnergyRechargeRateMultiplierAttribute()})
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.0f));
		Modifiers.Add(Modifier);
	}
}
