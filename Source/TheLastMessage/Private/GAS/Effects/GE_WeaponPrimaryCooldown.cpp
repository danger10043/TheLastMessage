#include "GAS/Effects/GE_WeaponPrimaryCooldown.h"
#include "GAS/WeaponGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_WeaponPrimaryCooldown::UGE_WeaponPrimaryCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.5f));
	FInheritedTagContainer Tags;
	Tags.AddTag(WeaponGameplayTags::Cooldown_Primary);
	// 생성자에서는 이름 없는 NewObject를 사용하는 FindOrAddComponent를 호출할 수 없다.
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("PrimaryCooldownTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}
