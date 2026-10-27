#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreResidualPowerRecovery.generated.h"

class UGameplayEffect;

/** 처치 결과를 받아 현재 KillHealAmount만큼 회복하고 종료하는 잔여 전력 회수 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CoreResidualPowerRecovery : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreResidualPowerRecovery();

protected:
	/** 처치당 고정 회복량을 적용하는 Instant GE. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|Healing")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	/** 처치 표시가 있는 피해 이벤트에만 반응한다. */
	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
