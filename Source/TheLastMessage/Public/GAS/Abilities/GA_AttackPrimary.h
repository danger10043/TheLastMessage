#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/GA_WeaponAttackBase.h"
#include "GA_AttackPrimary.generated.h"

/** 주무기 슬롯을 사용하는 공격 능력. 공통 공격 처리는 부모에서 구현한다. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API UGA_AttackPrimary : public UGA_WeaponAttackBase
{
	GENERATED_BODY()

public:
	UGA_AttackPrimary();
};
