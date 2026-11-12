#include "GAS/Abilities/GA_CoreHealBase.h"
#include "AbilitySystemComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "GameplayEffect.h"
#include "DataAsset/GameplayAssetSettings.h"

UGA_CoreHealBase::UGA_CoreHealBase()
{
	HealEffectClass = GetDefault<UGameplayAssetSettings>()->CoreHealEffect.LoadSynchronous();

	// 회복은 서버에서만 계산하고, 사망 중에는 실행하지 않는다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);

	// 플레이어가 실제 피해를 줬다는 이벤트로 실행된다.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CombatGameplayTags::Event_DamageDealt;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

bool UGA_CoreHealBase::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	return ActorInfo && Payload && Payload->EventTag == CombatGameplayTags::Event_DamageDealt
		&& AcceptsDamageEvent(*Payload) && Super::ShouldAbilityRespondToEvent(ActorInfo, Payload);
}

void UGA_CoreHealBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 이벤트 검사: 살아 있는 플레이어가 자기 자신이 아닌 대상에게 준 피해이고, 이 능력이 받는 종류여야 한다.
	APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Player) || !Player->HasAuthority() || Player->IsDead() || !IsValid(ASC)
		|| !TriggerEventData || TriggerEventData->EventTag != CombatGameplayTags::Event_DamageDealt
		|| !AcceptsDamageEvent(*TriggerEventData)
		|| TriggerEventData->Instigator != Player || !TriggerEventData->Target || TriggerEventData->Target == Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 플레이어 무기 피해(근접 또는 원거리 중 하나)만 회복 대상이다. 지원 로봇 탄은 공격자가 로봇이라 위에서 이미 빠진다.
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

	// 회복량 계산: 자식이 정한 공식으로 구하고, 최대 체력까지 부족한 양만큼만 회복한다.
	const UCombatAttributeSet* Attributes = ASC->GetSet<UCombatAttributeSet>();
	if (!IsValid(Player) || Player->IsDead() || !Attributes)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float Health = Attributes->GetHealth();
	const float MaxHealth = Attributes->GetMaxHealth();
	const float RawHeal = CalculateHealAmount(*Attributes, ActualDamage);
	if (!FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth) || !FMath::IsFinite(RawHeal)
		|| Health <= 0.f || MaxHealth <= Health || RawHeal <= 0.f || !HealEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float HealAmount = static_cast<float>(FMath::Min(
		static_cast<double>(RawHeal), static_cast<double>(MaxHealth) - Health));

	// 회복 GE 적용: 회복량을 SetByCaller로 넘기고, 적용에 실패하면 취소로 끝낸다.
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, HealEffectClass);
	if (!Spec.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(CoreGameplayTags::Data_HealAmount, HealAmount);
	const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !Applied.WasSuccessfullyApplied());
}
