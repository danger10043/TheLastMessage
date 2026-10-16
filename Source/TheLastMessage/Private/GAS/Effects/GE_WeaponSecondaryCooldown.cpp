#include "GAS/Effects/GE_WeaponSecondaryCooldown.h"
#include "GAS/WeaponGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_WeaponSecondaryCooldown::UGE_WeaponSecondaryCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.5f));
	FInheritedTagContainer Tags;
	Tags.AddTag(WeaponGameplayTags::Cooldown_Secondary);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SecondaryCooldownTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}
