#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_WeaponEnergyCost.generated.h"

/** 무기 공격 비용. 두 슬롯이 같은 공용 에너지를 쓰므로 하나의 GE로 처리한다. 비용은 SetByCaller(Data.EnergyCost)로 전달한다. */
UCLASS()
class THELASTMESSAGE_API UGE_WeaponEnergyCost : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WeaponEnergyCost();
};
