#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreAssistStrikeArm.generated.h"

class UMeleeAttackEventData;

/** 정상 종료한 근접 공격을 받아 보조 타격의 공격 정보와 피해를 준비하는 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CoreAssistStrikeArm : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreAssistStrikeArm();

protected:
	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	/** 원래 공격의 계산된 피해에 현재 AssistStrikeDamageRatio를 한 번 적용한다. */
	bool TryPrepareStrike(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload,
		const UMeleeAttackEventData*& OutAttack, float& OutDamage) const;
};
