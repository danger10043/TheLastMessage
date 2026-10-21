#include "GAS/WeaponSlotAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UWeaponSlotAttributeSet::UWeaponSlotAttributeSet()
{
	InitSharedMaxEnergy(100.0f);
	InitSharedEnergy(100.0f);
}

void UWeaponSlotAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (!FMath::IsFinite(NewValue)) NewValue = 0.0f;
	if (Attribute == GetSharedMaxEnergyAttribute()) NewValue = FMath::Max(0.0f, NewValue);
	else if (Attribute == GetSharedEnergyAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetSharedMaxEnergy());
}

void UWeaponSlotAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetSharedMaxEnergyAttribute() && GetSharedEnergy() > NewValue) SetSharedEnergy(NewValue);
}

void UWeaponSlotAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	SetSharedEnergy(FMath::Clamp(GetSharedEnergy(), 0.0f, GetSharedMaxEnergy()));
}

void UWeaponSlotAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SharedEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponSlotAttributeSet, SharedMaxEnergy, COND_None, REPNOTIFY_Always);
}

void UWeaponSlotAttributeSet::OnRep_SharedEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SharedEnergy, OldValue); }
void UWeaponSlotAttributeSet::OnRep_SharedMaxEnergy(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponSlotAttributeSet, SharedMaxEnergy, OldValue); }
