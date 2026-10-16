#include "GAS/WeaponGameplayTags.h"

namespace WeaponGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Input_Primary, "Input.Weapon.Primary");
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Primary, "Cooldown.Weapon.Primary");
	UE_DEFINE_GAMEPLAY_TAG(Event_MeleeHit, "Event.Weapon.MeleeHit");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Damage_PlayerMelee, "Damage.Category.Player.Melee");
}
