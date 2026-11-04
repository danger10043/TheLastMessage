#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/GA_CoreHealBase.h"
#include "GA_CoreResidualPowerRecovery.generated.h"

/** 처치 결과를 받아 최대 체력의 KillHealRatio만큼 회복하고 종료하는 잔여 전력 회수 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CoreResidualPowerRecovery : public UGA_CoreHealBase
{
	GENERATED_BODY()

protected:
	/** 처치 표시(Combat.Result.TargetKilled)가 있는 피해 이벤트에만 반응한다. */
	virtual bool AcceptsDamageEvent(const FGameplayEventData& Event) const override;

	/** 처치 시점의 최대 체력과 회복 비율로 회복량을 정한다. */
	virtual float CalculateHealAmount(const UCombatAttributeSet& Attributes, float ActualDamage) const override;
};
