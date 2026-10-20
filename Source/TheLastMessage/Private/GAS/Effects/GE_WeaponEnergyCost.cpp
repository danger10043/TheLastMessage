#include "GAS/Effects/GE_WeaponEnergyCost.h"
#include "GAS/WeaponSlotAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"

namespace
{
FGameplayModifierInfo EnergyCost(const FGameplayAttribute& Attribute)
{
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Cost;
	Cost.DataTag = WeaponGameplayTags::Data_EnergyCost;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Cost);
	return Modifier;
}
}

UGE_WeaponPrimaryEnergyCost::UGE_WeaponPrimaryEnergyCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(EnergyCost(UWeaponSlotAttributeSet::GetSharedEnergyAttribute()));
}

UGE_WeaponSecondaryEnergyCost::UGE_WeaponSecondaryEnergyCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(EnergyCost(UWeaponSlotAttributeSet::GetSharedEnergyAttribute()));
}
