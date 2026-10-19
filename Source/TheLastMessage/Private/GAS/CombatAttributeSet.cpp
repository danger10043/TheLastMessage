// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/CombatAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Player/CombatFeedbackComponent.h"

UCombatAttributeSet::UCombatAttributeSet()
{
	// 효과가 없는 배율은 1. 실제 시작 스탯은 초기화 GE에서 설정한다.
	InitCriticalDamageMultiplier(1.0f);
	InitDamageTakenMultiplier(1.0f);
	InitDamageDealtMultiplier(1.0f);
	InitMeleeDamageMultiplier(1.0f);
	InitChargeTimeMultiplier(1.0f);
	InitRangedEnergyCostMultiplier(1.0f);
	InitEnergyRechargeRateMultiplier(1.0f);
	InitMoveSpeedMultiplier(1.0f);
	InitHighHealthDamageMultiplier(1.0f);
	InitLowHealthDamageMultiplier(1.0f);

	// 코어가 부여하기 전에는 추가 회복과 보조 타격 피해가 없다.
	InitLifeStealRatio(0.0f);
	InitKillHealAmount(0.0f);
	InitAssistStrikeDamageRatio(0.0f);
}

void UCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UCombatAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, NewValue));
	}
}

void UCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	APlayerBase* Player = ASC
		? Cast<APlayerBase>(ASC->GetAvatarActor())
		: nullptr;

	bool bCheckDeath = false;
	bool bReceivedDamage = false;

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float IncomingDamage = GetIncomingDamage();

		// 임시 피해값은 처리 여부와 관계없이 비움
		SetIncomingDamage(0.0f);

		// 체력을 차감하는 이 지점에서 받는 피해 배율을 한 번만 적용한다. 0.9라면 피해 10% 감소다.
		const float Damage = IncomingDamage * FMath::Max(0.0f, GetDamageTakenMultiplier());
		if (FMath::IsFinite(Damage) && Damage > 0.0f && (!Player || !Player->IsDead()))
		{
			const float PreviousHealth = GetHealth();
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetMaxHealth()));
			bReceivedDamage = GetHealth() < PreviousHealth;
			
			bCheckDeath = true;
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// 체력을 직접 변경하는 GE에도 범위 제한 적용
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
		bCheckDeath = true;
		bReceivedDamage = Data.EvaluatedData.ModifierOp == EGameplayModOp::Additive && Data.EvaluatedData.Magnitude < 0.0f;
	}

	// Direct Health modifiers have already changed the attribute before this callback.
	if (bReceivedDamage && Player && !Player->IsDead())
	{
		if (UCombatFeedbackComponent* Feedback = Player->FindComponentByClass<UCombatFeedbackComponent>())
			Feedback->NotifyDamageReceived();
	}
	if (bCheckDeath && GetHealth() <= 0.0f && Player)
	{
		Player->HandleDeath(Player->GetDamageReactionSection(Data.EffectSpec.GetContext()));
	}
	else if (bReceivedDamage && GetHealth() > 0.0f && Player)
	{
		Player->HandleHitReaction(Player->GetDamageReactionSection(Data.EffectSpec.GetContext()));
	}
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UCombatAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Health, OldValue);
}

void UCombatAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MaxHealth, OldValue);
}

void UCombatAttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MoveSpeedMultiplier, OldValue);
}
