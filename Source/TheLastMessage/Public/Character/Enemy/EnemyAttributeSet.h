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

	float GetHealthPercent() const;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
