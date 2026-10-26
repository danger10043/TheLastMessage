#include "GAS/Effects/GE_CoreHeal.h"
#include "GAS/CombatAttributeSet.h"

namespace CoreGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Data_HealAmount, "Data.Core.HealAmount");
}

UGE_CoreHeal::UGE_CoreHeal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCombatAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Healing;
	Healing.DataTag = CoreGameplayTags::Data_HealAmount;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Healing);
	Modifiers.Add(Modifier);
}
