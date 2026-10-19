#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_WeaponEnergyCost.generated.h"

UCLASS()
class THELASTMESSAGE_API UGE_WeaponPrimaryEnergyCost : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WeaponPrimaryEnergyCost();
};

UCLASS()
class THELASTMESSAGE_API UGE_WeaponSecondaryEnergyCost : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WeaponSecondaryEnergyCost();
};
