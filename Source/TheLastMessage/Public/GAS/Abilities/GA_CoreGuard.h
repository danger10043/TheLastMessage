#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreGuard.generated.h"

/** 가드 키를 누르고 있는 동안 활성화되어 State.Guarding을 유지하고, 키를 떼면 종료하는 가드 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CoreGuard : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreGuard();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
};
