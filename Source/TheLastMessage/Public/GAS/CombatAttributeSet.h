// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "CombatAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class THELASTMESSAGE_API UCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UCombatAttributeSet();

	// CurrentValue 변경 전에 실행되는 함수. 값의 Clamping 용도로 사용.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// CurrentValue 변경 후에 실행되는 함수
	// 값의 변화 감지나, UI에 반영하기 위해 사용
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 이펙트가 적용 된 후에 실행되는 함수
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, MaxHealth);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData CriticalChance;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, CriticalChance);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData CriticalDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, CriticalDamageMultiplier);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData DamageTakenMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, DamageTakenMultiplier);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData DamageDealtMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, DamageDealtMultiplier);

	/** 근접 공격의 피해 배율. 1.0은 기본 피해, 1.2는 피해 20% 증가를 뜻한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData MeleeDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, MeleeDamageMultiplier);

	/** 최대 차지 시간에 적용할 배율. 1.0은 기본 시간, 0.8은 20% 단축이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData ChargeTimeMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, ChargeTimeMultiplier);

	/** 무기의 초당 에너지 충전량 배율. 1.0은 기본 충전량, 1.25는 25% 증가이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData EnergyRechargeRateMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, EnergyRechargeRateMultiplier);

	/** 기본 이동속도에 적용할 배율. 1.0은 기존 이동속도를 유지한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat", ReplicatedUsing = OnRep_MoveSpeedMultiplier)
	FGameplayAttributeData MoveSpeedMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, MoveSpeedMultiplier);

	/** 실제로 감소시킨 적 체력 중 회복할 비율. 0.0은 회복 없음, 0.1은 10% 회복이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData LifeStealRatio;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, LifeStealRatio);

	/** 적 처치 시 최대 체력을 기준으로 회복할 비율. 0.0은 회복 없음, 0.03은 최대 체력의 3% 회복이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData KillHealRatio;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, KillHealRatio);

	/** 원래 공격 피해 대비 보조 타격의 피해 비율. 0.0은 추가 피해 없음, 0.1은 10%이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData AssistStrikeDamageRatio;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, AssistStrikeDamageRatio);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData HighHealthThresholdRatio;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, HighHealthThresholdRatio);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData HighHealthDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, HighHealthDamageMultiplier);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData LowHealthThresholdRatio;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, LowHealthThresholdRatio);

	UPROPERTY(BlueprintReadOnly, Category = "Base Stat")
	FGameplayAttributeData LowHealthDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, LowHealthDamageMultiplier);

	// 피해 전달용 임시 Attribute. 피해 처리 단계에서 사용 후 0으로 초기화한다.
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UCombatAttributeSet, IncomingDamage);

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);

};
