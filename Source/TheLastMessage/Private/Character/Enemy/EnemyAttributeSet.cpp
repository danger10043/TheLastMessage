#include "Character/Enemy/EnemyAttributeSet.h"
#include "Character/Enemy/EnemyBase.h"
#include "GameplayEffectExtension.h"

float UEnemyAttributeSet::GetHealthPercent() const
{
	const float Max = GetMaxHealth();
	return Max > 0.f ? GetHealth() / Max : 0.f;
}

void UEnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute != GetHealthAttribute())
	{
		return;
	}

	SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

	AEnemyBase* Enemy = Cast<AEnemyBase>(Data.Target.GetAvatarActor());
	if (!Enemy || Enemy->IsDead())
	{
		return;
	}

	const float Damage = -Data.EvaluatedData.Magnitude;
	if (Damage > 0.f)
	{
		Enemy->OnEnemyDamaged.Broadcast(Damage);
	}

	if (GetHealth() <= 0.f)
	{
		Enemy->HandleDeath();
	}
}
