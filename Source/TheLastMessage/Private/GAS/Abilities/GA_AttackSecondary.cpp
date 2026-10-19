#include "GAS/Abilities/GA_AttackSecondary.h"
#include "GAS/Effects/GE_WeaponSecondaryCooldown.h"
#include "GAS/Effects/GE_WeaponEnergyCost.h"
#include "GAS/WeaponGameplayTags.h"

UGA_AttackSecondary::UGA_AttackSecondary()
{
	WeaponSlot = EWeaponSlot::Secondary;
	AttackCooldownTags.Reset();
	AttackCooldownTags.AddTag(WeaponGameplayTags::Cooldown_Secondary);
	CooldownEffectClass = UGE_WeaponSecondaryCooldown::StaticClass();
	CostEffectClass = UGE_WeaponSecondaryEnergyCost::StaticClass();
}
