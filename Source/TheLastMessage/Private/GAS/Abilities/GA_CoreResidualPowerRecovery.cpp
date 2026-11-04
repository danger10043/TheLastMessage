#include "GAS/Abilities/GA_CoreResidualPowerRecovery.h"
#include "Character/CombatDamageLibrary.h"
#include "GAS/CombatAttributeSet.h"

bool UGA_CoreResidualPowerRecovery::AcceptsDamageEvent(const FGameplayEventData& Event) const
{
	return Event.TargetTags.HasTagExact(CombatGameplayTags::Result_TargetKilled);
}

float UGA_CoreResidualPowerRecovery::CalculateHealAmount(const UCombatAttributeSet& Attributes, float ActualDamage) const
{
	return static_cast<float>(static_cast<double>(Attributes.GetMaxHealth()) * Attributes.GetKillHealRatio());
}
