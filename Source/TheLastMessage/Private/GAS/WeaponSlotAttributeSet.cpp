#include "GAS/WeaponSlotAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

FGameplayAttribute UWeaponSlotAttributeSet::EnergyAttribute(EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Primary ? GetPrimaryEnergyAttribute() :
		Slot == EWeaponSlot::Secondary ? GetSecondaryEnergyAttribute() : FGameplayAttribute();
}

FGameplayAttribute UWeaponSlotAttributeSet::MaxEnergyAttribute(EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Primary ? GetPrimaryMaxEnergyAttribute() :
		Slot == EWeaponSlot::Secondary ? GetSecondaryMaxEnergyAttribute() : FGameplayAttribute();
}

void UWeaponSlotAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (!FMath::IsFinite(NewValue)) NewValue = 0.0f;
	if (Attribute == GetPrimaryMaxEnergyAttribute() || Attribute == GetSecondaryMaxEnergyAttribute()) NewValue = FMath::Max(0.0f, NewValue);
	else if (Attribute == GetPrimaryEnergyAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetPrimaryMaxEnergy());
	else if (Attribute == GetSecondaryEnergyAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetSecondaryMaxEnergy());
}

void UWeaponSlotAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetPrimaryMaxEnergyAttribute() && GetPrimaryEnergy() > NewValue) SetPrimaryEnergy(NewValue);
	if (Attribute == GetSecondaryMaxEnergyAttribute() && GetSecondaryEnergy() > NewValue) SetSecondaryEnergy(NewValue);
}

void UWeaponSlotAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	SetPrimaryEnergy(FMath::Clamp(GetPrimaryEnergy(), 0.0f, GetPrimaryMaxEnergy()));
	SetSecondaryEnergy(FMath::Clamp(GetSecondaryEnergy(), 0.0f, GetSecondaryMaxEnergy()));
}

void UWeaponSlotAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, PrimaryEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, PrimaryMaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SecondaryEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SecondaryMaxEnergy, COND_None, REPNOTIFY_Always);
}

void UWeaponSlotAttributeSet::OnRep_PrimaryEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, PrimaryEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_PrimaryMaxEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, PrimaryMaxEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SecondaryEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SecondaryEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SecondaryMaxEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SecondaryMaxEnergy, OldValue); }
