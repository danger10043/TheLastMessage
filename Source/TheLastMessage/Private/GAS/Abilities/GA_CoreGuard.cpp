#include "GAS/Abilities/GA_CoreGuard.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/WeaponGameplayTags.h"

UGA_CoreGuard::UGA_CoreGuard()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);

	// 능력이 활성화된 동안만 가드 상태 태그를 붙이고, 종료·취소 시 GAS가 함께 제거한다.
	ActivationOwnedTags.AddTag(CoreGameplayTags::State_Guarding);
}

void UGA_CoreGuard::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 활성화 전에 키를 이미 뗐다면 놓친 해제 입력 대신 바로 종료한다.
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec || !Spec->InputPressed)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_CoreGuard::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}
