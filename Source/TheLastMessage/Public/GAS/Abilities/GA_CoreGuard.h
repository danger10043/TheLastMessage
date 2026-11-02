#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreGuard.generated.h"

/** 가드 키를 누르고 있는 동안 활성화되어 State.Guarding과 이동 정지를 유지하고, 키를 떼면 종료하는 가드 능력이다. */
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

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 가드 중에만 적용하는 이동 정지 GE의 핸들. 종료 시 제거한다. */
	FActiveGameplayEffectHandle RootEffectHandle;
};
