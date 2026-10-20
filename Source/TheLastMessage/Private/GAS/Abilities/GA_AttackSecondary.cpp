#include "GAS/Abilities/GA_AttackSecondary.h"
#include "GAS/Effects/GE_WeaponEnergyCost.h"

UGA_AttackSecondary::UGA_AttackSecondary()
{
	WeaponSlot = EWeaponSlot::Secondary;
	CostEffectClass = UGE_WeaponSecondaryEnergyCost::StaticClass();
}
