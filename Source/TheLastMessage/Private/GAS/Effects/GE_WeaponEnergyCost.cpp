#include "GAS/Effects/GE_WeaponEnergyCost.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"

UGE_WeaponEnergyCost::UGE_WeaponEnergyCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UWeaponSlotAttributeSet::GetSharedEnergyAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Cost;
	Cost.DataTag = WeaponGameplayTags::Data_EnergyCost;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Cost);
	Modifiers.Add(Modifier);
}
