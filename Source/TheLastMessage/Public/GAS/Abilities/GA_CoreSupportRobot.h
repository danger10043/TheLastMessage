#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CoreSupportRobot.generated.h"

class ACoreSupportRobot;
struct FOnAttributeChangeData;

/**
 * 지원 로봇 코어를 보유한 동안 켜져 있으며 로봇을 만들고 없애는 능력이다.
 * 플레이어가 피해를 주거나 받으면 로봇의 전투 상태를 유지한다.
 * 플레이어가 공격한 적과 플레이어를 공격한 적을 로봇에 전달하고, 누구를 노릴지는 로봇이 정한다.
 * 로봇이 준 피해는 공격자가 로봇이므로 전투 상태와 노릴 적을 갱신하지 않는다.
 */
UCLASS()
class THELASTMESSAGE_API UGA_CoreSupportRobot : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CoreSupportRobot();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot")
	TSubclassOf<ACoreSupportRobot> RobotClass;

	/** 플레이어가 피해를 주거나 받은 뒤 전투 상태를 유지하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot", meta = (ClampMin = "0.1"))
	float CombatDuration = 5.0f;

	/** 로봇이 처음 나타날 플레이어 기준 위치. */
	UPROPERTY(EditDefaultsOnly, Category = "Core|SupportRobot")
	FVector SpawnOffset = FVector(0.0f, 0.0f, 120.0f);

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** 코어를 얻거나 플레이어가 정해지면 입력 없이 바로 켠다. */
	void TryActivatePassive(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	void SpawnRobot();
	void DestroyRobot();

	void HandleDamageDealt(const FGameplayEventData* Payload);
	void HandleGuardBlocked(const FGameplayEventData* Payload);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleIncomingDamageChanged(const FOnAttributeChangeData& Data);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY(Transient)
	TObjectPtr<ACoreSupportRobot> Robot;

	FDelegateHandle DamageDealtHandle;
	FDelegateHandle GuardBlockedHandle;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle IncomingDamageHandle;
	FDelegateHandle DeadTagHandle;
};
