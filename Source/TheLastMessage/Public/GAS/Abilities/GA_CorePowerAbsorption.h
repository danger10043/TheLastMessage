#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CorePowerAbsorption.generated.h"

/** 실제 피해 결과를 받아 현재 LifeStealRatio만큼 회복하고 종료하는 전력 흡수 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CorePowerAbsorption : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CorePowerAbsorption();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
