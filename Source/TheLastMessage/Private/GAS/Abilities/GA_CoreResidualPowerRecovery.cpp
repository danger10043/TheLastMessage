#include "GAS/Abilities/GA_CoreResidualPowerRecovery.h"
#include "AbilitySystemComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "GameplayEffect.h"
#include "UObject/ConstructorHelpers.h"

UGA_CoreResidualPowerRecovery::UGA_CoreResidualPowerRecovery()
{
	static ConstructorHelpers::FClassFinder<UGameplayEffect> HealEffect(
		TEXT("/Game/Test_Jihun/Core/Stat/GE/GE_Core_Heal"));
	HealEffectClass = HealEffect.Class;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CombatGameplayTags::Event_DamageDealt;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

bool UGA_CoreResidualPowerRecovery::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	return ActorInfo && Payload && Payload->EventTag == CombatGameplayTags::Event_DamageDealt
		&& Payload->TargetTags.HasTagExact(CombatGameplayTags::Result_TargetKilled)
		&& Super::ShouldAbilityRespondToEvent(ActorInfo, Payload);
}

void UGA_CoreResidualPowerRecovery::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Player) || !Player->HasAuthority() || Player->IsDead() || !IsValid(ASC)
		|| !TriggerEventData || TriggerEventData->EventTag != CombatGameplayTags::Event_DamageDealt
		|| !TriggerEventData->TargetTags.HasTagExact(CombatGameplayTags::Result_TargetKilled)
		|| TriggerEventData->Instigator != Player || !TriggerEventData->Target || TriggerEventData->Target == Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bMelee = TriggerEventData->InstigatorTags.HasTagExact(WeaponGameplayTags::Damage_PlayerMelee);
	const bool bRanged = TriggerEventData->InstigatorTags.HasTagExact(WeaponGameplayTags::Damage_PlayerRanged);
	if (bMelee == bRanged || !FMath::IsFinite(TriggerEventData->EventMagnitude) || TriggerEventData->EventMagnitude <= 0.f
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 능력 실행 알림에서 취소되었다면 회복도 진행하지 않는다.
	if (!IsActive()) return;

	const UCombatAttributeSet* Attributes = IsValid(ASC) ? ASC->GetSet<UCombatAttributeSet>() : nullptr;
	if (!IsValid(Player) || Player->IsDead() || !Attributes)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float Health = Attributes->GetHealth();
	const float MaxHealth = Attributes->GetMaxHealth();
	const float Recovery = Attributes->GetKillHealAmount();
	if (!FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth) || !FMath::IsFinite(Recovery)
		|| Health <= 0.f || MaxHealth <= Health || Recovery <= 0.f || !HealEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 처치 시점의 고정 회복량을 사용하고, 최대 체력까지 부족한 양만 회복한다.
	const float HealAmount = FMath::Min(Recovery, MaxHealth - Health);
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, HealEffectClass);
	if (!Spec.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	static const FGameplayTag HealAmountTag = FGameplayTag::RequestGameplayTag(TEXT("Data.Core.HealAmount"));
	Spec.Data->SetSetByCallerMagnitude(HealAmountTag, HealAmount);
	const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Applied.WasSuccessfullyApplied());
}
