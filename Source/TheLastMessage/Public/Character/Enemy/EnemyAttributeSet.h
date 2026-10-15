#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.generated.h"

// 플레이어 AttributeSet의 매크로와 이름이 겹치지 않도록 접두사를 붙였다.
#define ENEMY_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class THELASTMESSAGE_API UEnemyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackPower;
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MoveSpeed;
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MoveSpeed)

	/**
	 * 받은 피해를 전달하는 임시(Meta) Attribute. 피해 GE가 여기에 값을 더하면
	 * PostGameplayEffectExecute가 체력으로 옮기고 0으로 되돌린다. (플레이어 CombatAttributeSet과 같은 방식)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData IncomingDamage;
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, IncomingDamage)

	/** 레벨에 따른 주는 피해 배율. 기본 1. (명세서 외 추가 Attribute) */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData DamageMultiplier;
	ENEMY_ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, DamageMultiplier)

	float GetHealthPercent() const;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
