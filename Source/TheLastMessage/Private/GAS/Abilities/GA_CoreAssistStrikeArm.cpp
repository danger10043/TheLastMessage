#include "GAS/Abilities/GA_CoreAssistStrikeArm.h"

#include "AbilitySystemComponent.h"
#include "Character/CombatDamageLibrary.h"
#include "Character/Player/PlayerBase.h"
#include "DataAsset/WeaponDataAsset.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

UGA_CoreAssistStrikeArm::UGA_CoreAssistStrikeArm()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(WeaponGameplayTags::State_Dead);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = CombatGameplayTags::Event_MeleeAttackFinished;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

bool UGA_CoreAssistStrikeArm::TryPrepareStrike(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload, const UMeleeAttackEventData*& OutAttack, float& OutDamage) const
{
	OutAttack = nullptr;
	OutDamage = 0.0f;
	const APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Player) || !Player->HasAuthority() || Player->IsDead() || !IsValid(ASC)
		|| ASC->GetAvatarActor() != Player || !Payload
		|| Payload->EventTag != CombatGameplayTags::Event_MeleeAttackFinished || Payload->Instigator != Player
		|| !Payload->InstigatorTags.HasTagExact(WeaponGameplayTags::Damage_PlayerMelee)
		|| Payload->InstigatorTags.HasTagExact(WeaponGameplayTags::Damage_PlayerRanged)) return false;

	const UMeleeAttackEventData* Attack = Cast<UMeleeAttackEventData>(Payload->OptionalObject.Get());
	if (!IsValid(Attack) || !IsValid(Attack->WeaponData)
		|| (Attack->WeaponData->WeaponType != EWeaponType::LightMelee && Attack->WeaponData->WeaponType != EWeaponType::HeavyMelee)) return false;

	const UCombatAttributeSet* Attributes = ASC->GetSet<UCombatAttributeSet>();
	const float Ratio = Attributes ? Attributes->GetAssistStrikeDamageRatio() : 0.0f;
	if (!FMath::IsFinite(Payload->EventMagnitude) || Payload->EventMagnitude <= 0.0f
		|| !FMath::IsFinite(Ratio) || Ratio <= 0.0f) return false;

	// 차지·치명타·공격자 배율은 원래 피해에 포함되어 있으므로 다시 적용하지 않는다.
	const double Damage = static_cast<double>(Payload->EventMagnitude) * Ratio;
	if (!FMath::IsFinite(Damage) || Damage <= 0.0 || Damage > MAX_flt) return false;
	OutAttack = Attack;
	OutDamage = static_cast<float>(Damage);
	return OutDamage > 0.0f;
}

bool UGA_CoreAssistStrikeArm::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	const UMeleeAttackEventData* Attack = nullptr;
	float Damage = 0.0f;
	return Super::ShouldAbilityRespondToEvent(ActorInfo, Payload) && TryPrepareStrike(ActorInfo, Payload, Attack, Damage);
}

void UGA_CoreAssistStrikeArm::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UMeleeAttackEventData* Attack = nullptr;
	float Damage = 0.0f;
	if (!TryPrepareStrike(ActorInfo, TriggerEventData, Attack, Damage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// 실행 알림 중 코어가 초기화돼도 원래 공격 정보는 이번 호출이 끝날 때까지 유지한다.
	TStrongObjectPtr<const UMeleeAttackEventData> AttackRecord(Attack);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive()) return;

	const APlayerBase* Player = ActorInfo ? Cast<APlayerBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsValid(Player) || Player->IsDead())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UE_LOG(LogTemp, VeryVerbose, TEXT("보조 타격 준비: 피해 %.3f, 무기 %s"), Damage, *GetNameSafe(AttackRecord->WeaponData));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
