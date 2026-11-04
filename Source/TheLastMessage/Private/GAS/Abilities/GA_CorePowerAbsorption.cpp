#include "GAS/Abilities/GA_CorePowerAbsorption.h"
#include "GAS/CombatAttributeSet.h"

float UGA_CorePowerAbsorption::CalculateHealAmount(const UCombatAttributeSet& Attributes, float ActualDamage) const
{
	return static_cast<float>(static_cast<double>(ActualDamage) * Attributes.GetLifeStealRatio());
}
