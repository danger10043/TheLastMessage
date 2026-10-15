#include "Character/Boss/BossAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Character/Boss/BossMonster.h"
#include "Character/CombatDamageLibrary.h"

float UBossAttributeSet::GetHealthPercent()
{
    const float MaxHealthValue = GetMaxHealth();

    if (MaxHealthValue <= 0.f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("BossAttributeSet::GetHealthPercent [%s] - 최대 체력이 0 이하라 체력 비율을 계산할 수 없습니다. MaxHealth: %f"),
            *GetName(),
            MaxHealthValue);
        return 0.f;
    }

    return FMath::Clamp(GetHealth() / MaxHealthValue, 0.f, 1.f);
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

bool UBossAttributeSet::PreGameplayEffectExecute(
    FGameplayEffectModCallbackData& Data)
{
    if (!Super::PreGameplayEffectExecute(Data))
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossAttributeSet::PreGameplayEffectExecute - 부모 처리에서 GE 변경을 거부했습니다."));
        return false;
    }
    if (Data.EvaluatedData.Attribute != GetHealthAttribute())
    {
        return true;
    }
    if (!FMath::IsFinite(Data.EvaluatedData.Magnitude))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossAttributeSet::PreGameplayEffectExecute - 체력 변경량이 유한한 수가 아닙니다."));
        return false;
    }

    ABossMonster* Boss = Cast<ABossMonster>(GetOwningActor());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error, TEXT("BossAttributeSet::PreGameplayEffectExecute - 소유자가 보스가 아닙니다."));
        return false;
    }
    if (Boss->GetBossState() == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossAttributeSet::PreGameplayEffectExecute - 사망 후 체력 변경을 차단합니다."));
        return false;
    }

    // 초기 스탯 Override와 양수 회복은 기존 처리를 유지한다.
    if (Data.EvaluatedData.ModifierOp != EGameplayModOp::Additive)
    {
        return true;
    }
    if (Data.EvaluatedData.Magnitude >= 0.f)
    {
        return true;
    }
    if (Boss->GetBossState() == EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossAttributeSet::PreGameplayEffectExecute - 실드 퍼즐 중 피해를 차단합니다."));
        return false;
    }
    if (Boss->GetBossState() == EBossState::Cinematic)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossAttributeSet::PreGameplayEffectExecute - 시네마틱 중 피해를 차단합니다."));
        return false;
    }
    if (GetMaxHealth() <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossAttributeSet::PreGameplayEffectExecute - 최대 체력이 초기화되지 않았습니다."));
        return false;
    }

    const float HealthFloor =
        GetMaxHealth() * Boss->GetDamageHealthFloorRatio();
    const float AllowedDamage = FMath::Max(0.f, GetHealth() - HealthFloor);
    const float RequestedDamage = -Data.EvaluatedData.Magnitude;
    Data.EvaluatedData.Magnitude = -FMath::Min(RequestedDamage, AllowedDamage);
    return true;
}

void UBossAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float RequestedDamage = GetIncomingDamage();
        SetIncomingDamage(0.f);

        if (Data.EvaluatedData.ModifierOp != EGameplayModOp::Additive
            || !FMath::IsFinite(RequestedDamage) || RequestedDamage <= 0.f)
        {
            return;
        }

        ABossMonster* Boss = Cast<ABossMonster>(Data.Target.GetAvatarActor());
        if (!IsValid(Boss) || !Boss->HasAuthority())
        {
            return;
        }

        const EBossState State = Boss->GetBossState();
        if (State == EBossState::Dead || State == EBossState::ShieldPuzzle
            || State == EBossState::Cinematic)
        {
            return;
        }

        const FGameplayTag ShieldTag = FGameplayTag::RequestGameplayTag(
            FName(TEXT("Boss.Immunity.Shield")), false);
        if (!ShieldTag.IsValid() || Data.Target.HasMatchingGameplayTag(ShieldTag))
        {
            return;
        }

        FGameplayTag Category;
        if (!UCombatDamageLibrary::TryGetDamageCategory(Data.EffectSpec, Category))
        {
            return;
        }
        if (Category.GetTagName() == FName(TEXT("Damage.Category.Player.Ranged"))
            && Boss->IsRangedImmune())
        {
            return;
        }

        const float CurrentHealthValue = GetHealth();
        const float MaxHealthValue = GetMaxHealth();
        const float FloorRatio = Boss->GetDamageHealthFloorRatio();
        if (!FMath::IsFinite(CurrentHealthValue) || !FMath::IsFinite(MaxHealthValue)
            || MaxHealthValue <= 0.f || !FMath::IsFinite(FloorRatio)
            || FloorRatio < 0.f || FloorRatio > 1.f)
        {
            return;
        }

        const float HealthFloor = MaxHealthValue * FloorRatio;
        const float AllowedDamage = FMath::Max(0.f, CurrentHealthValue - HealthFloor);
        const float AppliedDamage = FMath::Min(RequestedDamage, AllowedDamage);
        SetHealth(FMath::Clamp(CurrentHealthValue - AppliedDamage, 0.f, MaxHealthValue));

        Boss->CheckBossPhaseThreshold();
        if (GetHealth() <= 0.f)
        {
            Boss->HandleBossDeath();
        }
        return;
    }

    if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.f, FMath::Max(0.f, GetMaxHealth())));
        return;
    }
    if (Data.EvaluatedData.Attribute != GetHealthAttribute())
    {
        return;
    }

    SetHealth(FMath::Clamp(GetHealth(), 0.f, FMath::Max(0.f, GetMaxHealth())));

    // 초기 Override와 회복은 페이즈 전환을 요청하지 않는다.
    if (Data.EvaluatedData.ModifierOp != EGameplayModOp::Additive)
    {
        return;
    }
    if (Data.EvaluatedData.Magnitude > 0.f)
    {
        return;
    }

    ABossMonster* Boss = Cast<ABossMonster>(GetOwningActor());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Error,
            TEXT("BossAttributeSet::PostGameplayEffectExecute - 소유자가 보스가 아닙니다."));
        return;
    }

    // 페이즈 임계값 확인을 사망 판단보다 먼저 수행한다.
    Boss->CheckBossPhaseThreshold();

    if (GetHealth() <= 0.f)
    {
        Boss->HandleBossDeath();
    }
}