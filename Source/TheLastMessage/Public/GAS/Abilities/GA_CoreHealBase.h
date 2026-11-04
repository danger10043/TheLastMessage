#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreHealBase.generated.h"

class UCombatAttributeSet;
class UGameplayEffect;

/**
 * 플레이어가 무기로 적에게 실제 피해를 줬을 때(Event.Combat.DamageDealt) 체력을 회복하고 바로 끝나는 능력의 공통 부모다.
 * 이벤트 검사, 회복 GE 적용, 최대 체력까지만 회복하는 처리를 여기서 하고,
 * 어떤 이벤트에 반응할지와 회복량 공식만 자식(전력 흡수·잔여 전력 회수)이 정한다.
 */
UCLASS(Abstract)
class THELASTMESSAGE_API UGA_CoreHealBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreHealBase();

protected:
	/** 계산된 회복량을 적용하는 Instant GE. 회복량은 Data.Core.HealAmount SetByCaller로 넘긴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|Healing")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	/** 피해 이벤트 중 이 능력이 반응할 것만 고른다. 기본은 모든 피해 이벤트다. */
	virtual bool AcceptsDamageEvent(const FGameplayEventData& Event) const { return true; }

	/**
	 * 최대 체력으로 자르기 전의 회복량을 계산한다. 0 이하면 회복하지 않는다.
	 * ActualDamage는 적에게 실제로 들어간 피해량(이벤트 EventMagnitude)이다.
	 */
	virtual float CalculateHealAmount(const UCombatAttributeSet& Attributes, float ActualDamage) const
		PURE_VIRTUAL(UGA_CoreHealBase::CalculateHealAmount, return 0.f;);

	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
