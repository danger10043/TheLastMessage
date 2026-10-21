#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponSlotAttributeSet.generated.h"

/**
 * 두 무기 슬롯이 함께 사용하는 공용 에너지. 플레이어 ASC에만 존재한다.
 * 시작 최대 에너지·충전 속도는 APlayerBase의 InitialSharedMaxEnergy·SharedEnergyRecharge* 값에서 정한다.
 */
UCLASS()
class THELASTMESSAGE_API UWeaponSlotAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	UWeaponSlotAttributeSet();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SharedEnergy, Category="Weapon|Energy")
	FGameplayAttributeData SharedEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UWeaponSlotAttributeSet, SharedEnergy);
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SharedMaxEnergy, Category="Weapon|Energy")
	FGameplayAttributeData SharedMaxEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(UWeaponSlotAttributeSet, SharedMaxEnergy);
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UFUNCTION() void OnRep_SharedEnergy(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SharedMaxEnergy(const FGameplayAttributeData& OldValue);
};
