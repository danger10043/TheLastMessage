#include "GAS/Abilities/GA_CorePowerAbsorption.h"
#include "AbilitySystemComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/Effects/GE_CoreHeal.h"
#include "GAS/WeaponGameplayTags.h"

UGA_CorePowerAbsorption::UGA_CorePowerAbsorption()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CombatGameplayTags::Event_DamageDealt;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UGA_CorePowerAbsorption::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Player) || !Player->HasAuthority() || Player->IsDead() || !IsValid(ASC)
		|| !TriggerEventData || TriggerEventData->EventTag != CombatGameplayTags::Event_DamageDealt
		|| TriggerEventData->Instigator != Player || !TriggerEventData->Target || TriggerEventData->Target == Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bMelee = TriggerEventData->InstigatorTags.HasTagExact(WeaponGameplayTags::Damage_PlayerMelee);
	const bool bRanged = TriggerEventData->InstigatorTags.HasTagExact(WeaponGameplayTags::Damage_PlayerRanged);
	const float ActualDamage = TriggerEventData->EventMagnitude;
	if (bMelee == bRanged || !FMath::IsFinite(ActualDamage) || ActualDamage <= 0.f
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
	const float Ratio = Attributes->GetLifeStealRatio();
	if (!FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth) || !FMath::IsFinite(Ratio)
		|| Health <= 0.f || MaxHealth <= Health || Ratio <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 명중 시점의 흡혈 비율을 사용하고, 최대 체력까지 부족한 양만 회복한다.
	const float HealAmount = static_cast<float>(FMath::Min(
		static_cast<double>(ActualDamage) * Ratio, static_cast<double>(MaxHealth) - Health));
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, UGE_CoreHeal::StaticClass());
	if (!Spec.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(CoreGameplayTags::Data_HealAmount, HealAmount);
	const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Applied.WasSuccessfullyApplied());
}
