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

	AEnemyBase* Enemy = Cast<AEnemyBase>(Data.Target.GetAvatarActor());
	float Damage = 0.f;

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// 피해 GE가 IncomingDamage에 더한 값을 읽고 0으로 되돌린 뒤 체력에 반영한다.
		Damage = FMath::Max(GetIncomingDamage(), 0.f);
		SetIncomingDamage(0.f);

		if (Enemy && Enemy->IsDead())
		{
			return;
		}

		if (Damage > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Health를 직접 바꾸는 GE(회복·테스트용)도 처리한다.
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		Damage = FMath::Max(-Data.EvaluatedData.Magnitude, 0.f);
	}
	else
	{
		return;
	}

	if (!Enemy || Enemy->IsDead())
	{
		return;
	}

	if (Damage > 0.f)
	{
		Enemy->OnEnemyDamaged.Broadcast(Damage);
	}

	if (GetHealth() <= 0.f)
	{
		Enemy->HandleDeath();
	}
}
