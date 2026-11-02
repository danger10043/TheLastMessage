// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/CombatAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Character/Player/PlayerBase.h"
#include "Character/Player/CombatFeedbackComponent.h"
#include "Core/CoreGameplayTags.h"
#include "GAS/WeaponSlotAttributeSet.h"

namespace
{
	// 코어 강화가 겹치거나 커브 데이터가 잘못되어도 전투가 깨지지 않도록 두는 안전 하한이다.
	// 밸런스 수치가 아니라 데이터 실수에 대한 보호 장치이며, 기획 확정 시 함께 조정한다.
	constexpr float MinDamageTakenMultiplier = 0.3f;	// 받는 피해는 최소 30%까지만 감소한다.
	constexpr float MinChargeTimeMultiplier = 0.2f;		// 최대 차지 시간은 최소 20%까지만 단축한다.

	// 가드 중이면 공용 에너지를 피해만큼 소모해 막고, 막은 양을 반환한다. 에너지가 부족하면 남은 에너지만큼만 막는다.
	float BlockDamageWithGuardEnergy(UAbilitySystemComponent* ASC, APlayerBase* Player, float Damage)
	{
		const UWeaponSlotAttributeSet* EnergySet = ASC ? ASC->GetSet<UWeaponSlotAttributeSet>() : nullptr;
		if (!EnergySet || !ASC->HasMatchingGameplayTag(CoreGameplayTags::State_Guarding))
		{
			return 0.0f;
		}

		const float Energy = EnergySet->GetSharedEnergy();
		const float Blocked = FMath::Min(Damage, FMath::Max(0.0f, Energy));
		if (Blocked <= 0.0f)
		{
			return 0.0f;
		}

		ASC->SetNumericAttributeBase(UWeaponSlotAttributeSet::GetSharedEnergyAttribute(), Energy - Blocked);
		// 무기 사용과 같은 대기 시간 뒤에 에너지가 다시 충전된다.
		if (Player) Player->NotifyWeaponEnergySpent();
		return Blocked;
	}
}

UCombatAttributeSet::UCombatAttributeSet()
{
	// 효과가 없는 배율은 1. 실제 시작 스탯은 초기화 GE에서 설정한다.
	InitCriticalDamageMultiplier(1.0f);
	InitDamageTakenMultiplier(1.0f);
	InitDamageDealtMultiplier(1.0f);
	InitMeleeDamageMultiplier(1.0f);
	InitChargeTimeMultiplier(1.0f);
	InitEnergyRechargeRateMultiplier(1.0f);
	InitMoveSpeedMultiplier(1.0f);
	InitHighHealthDamageMultiplier(1.0f);
	InitLowHealthDamageMultiplier(1.0f);

	// 코어가 부여하기 전에는 추가 회복과 보조 타격 피해가 없다.
	InitLifeStealRatio(0.0f);
	InitKillHealRatio(0.0f);
	InitAssistStrikeDamageRatio(0.0f);

	// 연속 발사 효과가 없으면 원래 피해의 단발 공격이다.
	InitRangedBurstShotCount(1.0f);
	InitRangedBurstTotalDamageMultiplier(1.0f);
	InitRangedBurstInterval(0.1f);
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
	else if (Attribute == GetDamageTakenMultiplierAttribute())
	{
		NewValue = FMath::Max(MinDamageTakenMultiplier, NewValue);
	}
	else if (Attribute == GetChargeTimeMultiplierAttribute())
	{
		NewValue = FMath::Max(MinChargeTimeMultiplier, NewValue);
	}
	else if (Attribute == GetRangedBurstShotCountAttribute())
	{
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(1.0f, FMath::RoundToFloat(NewValue)) : 1.0f;
	}
	else if (Attribute == GetRangedBurstIntervalAttribute())
	{
		// 타이머가 실행될 수 있도록 양수 간격을 유지한다.
		NewValue = FMath::IsFinite(NewValue) ? FMath::Max(0.01f, NewValue) : 0.1f;
	}
	else if (Attribute == GetCriticalChanceAttribute()
		|| Attribute == GetHighHealthThresholdRatioAttribute()
		|| Attribute == GetLowHealthThresholdRatioAttribute())
	{
		// 확률과 체력 비율은 0~1 범위만 의미가 있다.
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	}
	else if (Attribute == GetDamageDealtMultiplierAttribute()
		|| Attribute == GetMeleeDamageMultiplierAttribute()
		|| Attribute == GetCriticalDamageMultiplierAttribute()
		|| Attribute == GetHighHealthDamageMultiplierAttribute()
		|| Attribute == GetLowHealthDamageMultiplierAttribute()
		|| Attribute == GetEnergyRechargeRateMultiplierAttribute()
		|| Attribute == GetMoveSpeedMultiplierAttribute()
		|| Attribute == GetLifeStealRatioAttribute()
		|| Attribute == GetKillHealRatioAttribute()
		|| Attribute == GetRangedBurstTotalDamageMultiplierAttribute()
		|| Attribute == GetAssistStrikeDamageRatioAttribute())
	{
		// 음수 배율·회복량은 피해 회복이나 역방향 이동 같은 의도하지 않은 동작을 만든다.
		NewValue = FMath::Max(0.0f, NewValue);
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
		const float RawIncomingDamage = GetIncomingDamage();

		// 임시 피해값은 처리 여부와 관계없이 비움
		SetIncomingDamage(0.0f);

		// 체력을 차감하는 이 지점에서 받는 피해 배율을 한 번만 적용한다. 0.9라면 피해 10% 감소다.
		const float Damage = RawIncomingDamage * FMath::Max(0.0f, GetDamageTakenMultiplier());
		if (FMath::IsFinite(Damage) && Damage > 0.0f && (!Player || !Player->IsDead()))
		{
			// 가드로 막은 양을 뺀 나머지만 체력에서 차감한다. 전부 막으면 체력이 줄지 않아 피격 반응도 없다.
			const float BlockedDamage = BlockDamageWithGuardEnergy(ASC, Player, Damage);
			const float RemainingDamage = Damage - BlockedDamage;
			const float PreviousHealth = GetHealth();
			SetHealth(FMath::Clamp(GetHealth() - RemainingDamage, 0.0f, GetMaxHealth()));
			bReceivedDamage = GetHealth() < PreviousHealth;

			// 체력 반영 후에 알려 연출·회복 능력이 막은 결과와 현재 체력을 함께 볼 수 있게 한다.
			if (BlockedDamage > 0.0f)
			{
				FGameplayEventData Payload;
				Payload.EventTag = CoreGameplayTags::Event_GuardBlocked;
				Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
				Payload.Target = ASC->GetAvatarActor();
				Payload.EventMagnitude = BlockedDamage;
				Payload.ContextHandle = Data.EffectSpec.GetContext();
				if (RemainingDamage > 0.0f) Payload.TargetTags.AddTag(CoreGameplayTags::Result_GuardPartial);
				ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
			}

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

bool UCombatAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
 if (!Super::PreGameplayEffectExecute(Data)) return false;
 const auto* ASC = GetOwningAbilitySystemComponent();
 const auto* Player = ASC ? Cast<APlayerBase>(ASC->GetAvatarActor()) : nullptr;
 if (!Player || !Player->IsRespawnProtected()) return true;
 if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute() && Data.EvaluatedData.Magnitude > 0.f) return false;
 if (Data.EvaluatedData.Attribute == GetHealthAttribute())
 {
  const float Value = Data.EvaluatedData.Magnitude;
  switch (Data.EvaluatedData.ModifierOp)
  {
   case EGameplayModOp::Additive: return Value >= 0.f;
   case EGameplayModOp::Multiplicitive: return GetHealth()*Value >= GetHealth();
   case EGameplayModOp::Division: return Value > 0.f && GetHealth()/Value >= GetHealth();
   case EGameplayModOp::Override: return Value >= GetHealth();
   default: break;
  }
 }
 return true;
}
