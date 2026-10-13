#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BossAttributeSet.generated.h"

#define BOSS_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class THELASTMESSAGE_API UBossAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FGameplayAttributeData Health;
    BOSS_ATTRIBUTE_ACCESSORS(UBossAttributeSet, Health)

    UPROPERTY()
    FGameplayAttributeData MaxHealth;
    BOSS_ATTRIBUTE_ACCESSORS(UBossAttributeSet, MaxHealth)

    UPROPERTY()
    FGameplayAttributeData AttackPower;
    BOSS_ATTRIBUTE_ACCESSORS(UBossAttributeSet, AttackPower)

    UPROPERTY()
    FGameplayAttributeData Defense;
    BOSS_ATTRIBUTE_ACCESSORS(UBossAttributeSet, Defense)

    UPROPERTY()
    FGameplayAttributeData MoveSpeed;
    BOSS_ATTRIBUTE_ACCESSORS(UBossAttributeSet, MoveSpeed)

    float GetHealthPercent();
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};

#undef BOSS_ATTRIBUTE_ACCESSORS