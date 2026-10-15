#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_WeaponPrimaryCooldown.generated.h"

/** 실제 지속 시간은 무기 데이터의 AttackInterval로 설정한다. */
UCLASS()
class THELASTMESSAGE_API UGE_WeaponPrimaryCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_WeaponPrimaryCooldown();
};
