#include "GAS/WeaponSlotAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UWeaponSlotAttributeSet::UWeaponSlotAttributeSet()
{
	InitSharedMaxEnergy(100.0f);
	InitSharedEnergy(100.0f);
}

FGameplayAttribute UWeaponSlotAttributeSet::EnergyAttribute(EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Primary || Slot == EWeaponSlot::Secondary ? GetSharedEnergyAttribute() : FGameplayAttribute();
}

FGameplayAttribute UWeaponSlotAttributeSet::MaxEnergyAttribute(EWeaponSlot Slot)
{
	return Slot == EWeaponSlot::Primary || Slot == EWeaponSlot::Secondary ? GetSharedMaxEnergyAttribute() : FGameplayAttribute();
}

void UWeaponSlotAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (!FMath::IsFinite(NewValue)) NewValue = 0.0f;
	if (Attribute == GetSharedMaxEnergyAttribute()) NewValue = FMath::Max(0.0f, NewValue);
	else if (Attribute == GetSharedEnergyAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetSharedMaxEnergy());
	if (Attribute == GetPrimaryMaxEnergyAttribute() || Attribute == GetSecondaryMaxEnergyAttribute()) NewValue = FMath::Max(0.0f, NewValue);
	else if (Attribute == GetPrimaryEnergyAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetPrimaryMaxEnergy());
	else if (Attribute == GetSecondaryEnergyAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetSecondaryMaxEnergy());
}

void UWeaponSlotAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetSharedMaxEnergyAttribute() && GetSharedEnergy() > NewValue) SetSharedEnergy(NewValue);
	if (Attribute == GetPrimaryMaxEnergyAttribute() && GetPrimaryEnergy() > NewValue) SetPrimaryEnergy(NewValue);
	if (Attribute == GetSecondaryMaxEnergyAttribute() && GetSecondaryEnergy() > NewValue) SetSecondaryEnergy(NewValue);
}

void UWeaponSlotAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	SetSharedEnergy(FMath::Clamp(GetSharedEnergy(), 0.0f, GetSharedMaxEnergy()));
	SetPrimaryEnergy(FMath::Clamp(GetPrimaryEnergy(), 0.0f, GetPrimaryMaxEnergy()));
	SetSecondaryEnergy(FMath::Clamp(GetSecondaryEnergy(), 0.0f, GetSecondaryMaxEnergy()));
}

void UWeaponSlotAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SharedEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SharedMaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, PrimaryEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, PrimaryMaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SecondaryEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SecondaryMaxEnergy, COND_None, REPNOTIFY_Always);
}

void UWeaponSlotAttributeSet::OnRep_PrimaryEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, PrimaryEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SharedEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SharedEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SharedMaxEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SharedMaxEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_PrimaryMaxEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, PrimaryMaxEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SecondaryEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SecondaryEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SecondaryMaxEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SecondaryMaxEnergy, OldValue); }
