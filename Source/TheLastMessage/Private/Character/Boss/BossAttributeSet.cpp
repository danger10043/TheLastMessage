#include "Character/Boss/BossAttributeSet.h"
#include "GameplayEffectExtension.h"

float UBossAttributeSet::GetHealthPercent()
{
    return GetMaxHealth() > 0.f
        ? FMath::Clamp(GetHealth() / GetMaxHealth(), 0.f, 1.f)
        : 0.f;
}

void UBossAttributeSet::PreAttributeChange(
    const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(0.f, GetMaxHealth()));
    }
    else if (Attribute == GetMaxHealthAttribute()
        || Attribute == GetMoveSpeedAttribute())
    {
        NewValue = FMath::Max(0.f, NewValue);
    }
}

void UBossAttributeSet::PostGameplayEffectExecute(
    const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute()
        || Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.f, FMath::Max(0.f, GetMaxHealth())));
    }

    // 페이즈 임계값 고정·실드 피해 차단·사망 연결은 전투 구현 단계에서 추가한다.
}