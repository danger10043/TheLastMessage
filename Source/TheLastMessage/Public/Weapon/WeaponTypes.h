#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.generated.h"

/** Weapon categories defined in section 3.1 of the integrated class specification. */
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	LightMelee UMETA(DisplayName = "Light Melee"),
	HeavyMelee UMETA(DisplayName = "Heavy Melee"),
	LightRanged UMETA(DisplayName = "Light Ranged"),
	HeavyRanged UMETA(DisplayName = "Heavy Ranged")
};

/** Equipment slots: left-click primary and right-click secondary. */
UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	None UMETA(DisplayName = "None"),
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary")
};
