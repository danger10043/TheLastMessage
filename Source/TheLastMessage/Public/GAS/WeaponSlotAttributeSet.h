#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponSlotAttributeSet.generated.h"

/** Equipped weapon energy lives only on the player's ASC. */
UCLASS()
class THELASTMESSAGE_API UWeaponSlotAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static FGameplayAttribute EnergyAttribute(EWeaponSlot Slot);
	static FGameplayAttribute MaxEnergyAttribute(EWeaponSlot Slot);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PrimaryEnergy, Category="Weapon|Energy")
	FGameplayAttributeData PrimaryEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UWeaponSlotAttributeSet, PrimaryEnergy);
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PrimaryMaxEnergy, Category="Weapon|Energy")
	FGameplayAttributeData PrimaryMaxEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UWeaponSlotAttributeSet, PrimaryMaxEnergy);
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SecondaryEnergy, Category="Weapon|Energy")
	FGameplayAttributeData SecondaryEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UWeaponSlotAttributeSet, SecondaryEnergy);
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SecondaryMaxEnergy, Category="Weapon|Energy")
	FGameplayAttributeData SecondaryMaxEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UWeaponSlotAttributeSet, SecondaryMaxEnergy);
private:
	UFUNCTION() void OnRep_PrimaryEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_PrimaryMaxEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SecondaryMaxEnergy(const FGameplayAttributeData& OldValue);
};
