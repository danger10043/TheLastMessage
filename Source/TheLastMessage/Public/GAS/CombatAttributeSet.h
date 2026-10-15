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

};
