#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/GA_CoreHealBase.h"
#include "GA_CorePowerAbsorption.generated.h"

/** 실제 피해 결과를 받아 현재 LifeStealRatio만큼 회복하고 종료하는 전력 흡수 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CorePowerAbsorption : public UGA_CoreHealBase
{
	GENERATED_BODY()

protected:
	/** 명중 시점의 흡혈 비율로, 적에게 실제로 준 피해의 일정 비율을 회복한다. */
	virtual float CalculateHealAmount(const UCombatAttributeSet& Attributes, float ActualDamage) const override;
};
