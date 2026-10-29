#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreAssistStrikeArm.generated.h"

class UMeleeAttackEventData;
class ACoreAssistAfterimage;

/** 정상 종료한 근접 공격의 정보와 피해로 독립된 보조 타격 잔상을 생성하는 능력이다. */
UCLASS()
class THELASTMESSAGE_API UGA_CoreAssistStrikeArm : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreAssistStrikeArm();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Core|AssistStrike")
	TSubclassOf<ACoreAssistAfterimage> AfterimageClass;

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
