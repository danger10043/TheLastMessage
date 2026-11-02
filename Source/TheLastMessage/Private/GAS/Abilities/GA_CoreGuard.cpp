#include "GAS/Abilities/GA_CoreGuard.h"
#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/Effects/GE_CoreGuardRoot.h"
#include "GAS/WeaponGameplayTags.h"

UGA_CoreGuard::UGA_CoreGuard()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	// 공격·피격 경직·그래플 중에는 가드를 시작하지 않는다.
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_HitReacting);
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Grappling);

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
		return;
	}

	const FGameplayEffectSpecHandle RootSpec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, UGE_CoreGuardRoot::StaticClass());
	if (RootSpec.IsValid())
	{
		RootEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, RootSpec);
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

void UGA_CoreGuard::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 종료·취소 경로와 관계없이 이동·충전 정지를 해제한다.
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && RootEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(RootEffectHandle);

		// 무기 사용 후와 같이 가드를 푼 뒤에도 대기 시간이 지나야 에너지가 충전된다.
		if (APlayerBase* Player = Cast<APlayerBase>(ActorInfo->AvatarActor.Get()))
		{
			Player->NotifyWeaponEnergySpent();
		}
	}
	RootEffectHandle.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
