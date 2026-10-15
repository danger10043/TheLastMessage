// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/CombatAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Character/Player/PlayerBase.h"

UCombatAttributeSet::UCombatAttributeSet()
{
	// 효과가 없는 배율은 1. 실제 시작 스탯은 초기화 GE에서 설정한다.
	InitCriticalDamageMultiplier(1.0f);
	InitDamageTakenMultiplier(1.0f);
	InitDamageDealtMultiplier(1.0f);
	InitHighHealthDamageMultiplier(1.0f);
	InitLowHealthDamageMultiplier(1.0f);

	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitCriticalChance(0.2f);

	InitHighHealthThresholdRatio(0.9f);
	InitLowHealthThresholdRatio(0.3f);
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

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();

		// 임시 피해값은 처리 여부와 관계없이 비움
		SetIncomingDamage(0.0f);

		if (Damage > 0.0f && (!Player || !Player->IsDead()))
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.0f, GetMaxHealth()));
			
			bCheckDeath = true;
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// 체력을 직접 변경하는 GE에도 범위 제한 적용
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
		bCheckDeath = true;
	}

	if (bCheckDeath && GetHealth() <= 0.0f && Player)
	{
		Player->HandleDeath();
	}
}

void UCombatAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCombatAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UCombatAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, Health, OldValue);
}

void UCombatAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCombatAttributeSet, MaxHealth, OldValue);
}
