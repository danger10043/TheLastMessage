#include "GAS/Effects/GE_WeaponMeleeDamage.h"
#include "AbilitySystemComponent.h"
#include "Character/Enemy/EnemyBase.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "GAS/CombatAttributeSet.h"
#include "GAS/WeaponGameplayTags.h"

UGE_WeaponMeleeDamage::UGE_WeaponMeleeDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UWeaponMeleeDamageExecution::StaticClass();
	Executions.Add(Execution);
}

void UWeaponMeleeDamageExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& Parameters,
	FGameplayEffectCustomExecutionOutput& Output) const
{
	UAbilitySystemComponent* SourceASC = Parameters.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = Parameters.GetTargetAbilitySystemComponent();
	if (!SourceASC || !TargetASC) return;

	const AEnemyBase* Enemy = Cast<AEnemyBase>(TargetASC->GetAvatarActor());
	if (!Enemy || Enemy->IsDead() || !TargetASC->GetSet<UEnemyAttributeSet>()) return;

	const FGameplayEffectSpec& Spec = Parameters.GetOwningSpec();
	float Damage = FMath::Max(0.0f, Spec.GetSetByCallerMagnitude(WeaponGameplayTags::Data_Damage, false, 0.0f));
	if (SourceASC->HasAttributeSetForAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()))
	{
		Damage *= FMath::Max(0.0f, SourceASC->GetNumericAttribute(UCombatAttributeSet::GetDamageDealtMultiplierAttribute()));
	}
	if (Damage > 0.0f)
	{
		Output.AddOutputModifier(FGameplayModifierEvaluatedData(
			UEnemyAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -Damage));
	}
}
