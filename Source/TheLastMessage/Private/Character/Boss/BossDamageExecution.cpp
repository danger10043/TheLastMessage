#include "Character/Boss/BossDamageExecution.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

void UBossDamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    UAbilitySystemComponent* TargetASC =
        ExecutionParams.GetTargetAbilitySystemComponent();
    if (!IsValid(TargetASC))
    {
        UE_LOG(LogTemp, Error, TEXT("BossDamageExecution - 대상 ASC가 없습니다."));
        return;
    }
    ABossMonster* Boss = Cast<ABossMonster>(TargetASC->GetAvatarActor());
    if (!IsValid(Boss))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageExecution - 대상 Avatar가 보스가 아닙니다."));
        return;
    }
    if (!Boss->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossDamageExecution - 실행 권한이 없습니다."));
        return;
    }
    if (Boss->GetBossState() == EBossState::Dead)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossDamageExecution - 사망한 보스의 피해를 차단합니다."));
        return;
    }
    if (Boss->GetBossState() == EBossState::ShieldPuzzle)
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossDamageExecution - 실드 퍼즐 중 피해를 차단합니다."));
        return;
    }

    const FGameplayTag ShieldTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Boss.Immunity.Shield")), false);
    if (!ShieldTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BossDamageExecution - 실드 면역 태그가 등록되지 않았습니다."));
        return;
    }
    if (TargetASC->HasMatchingGameplayTag(ShieldTag))
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossDamageExecution - 실드 면역으로 피해를 차단합니다."));
        return;
    }

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    const FName CategoryNames[] = {
        FName(TEXT("Damage.Category.Gimmick")),
        FName(TEXT("Damage.Category.Player.Melee")),
        FName(TEXT("Damage.Category.Player.Ranged"))
    };
    int32 CategoryCount = 0;
    bool bPlayerRanged = false;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FGameplayTag CategoryTag =
            FGameplayTag::RequestGameplayTag(CategoryNames[Index], false);
        if (!CategoryTag.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("BossDamageExecution - 피해 분류 태그가 등록되지 않았습니다: %s"),
                *CategoryNames[Index].ToString());
            return;
        }
        if (Spec.GetDynamicAssetTags().HasTagExact(CategoryTag))
        {
            ++CategoryCount;
            if (Index == 2)
            {
                bPlayerRanged = true;
            }
        }
    }
    if (CategoryCount != 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageExecution - 피해 분류는 정확히 하나여야 합니다."));
        return;
    }
    if (bPlayerRanged)
    {
        if (Boss->IsRangedImmune())
        {
            UE_LOG(LogTemp, Verbose, TEXT("BossDamageExecution - 플레이어 원거리 피해 면역입니다."));
            return;
        }
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Data.Damage")), false);
    if (!DamageTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BossDamageExecution - Data.Damage 태그가 등록되지 않았습니다."));
        return;
    }
    const float FinalDamage = Spec.GetSetByCallerMagnitude(DamageTag, false, -1.f);
    if (!FMath::IsFinite(FinalDamage))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageExecution - 피해량이 유한한 수가 아닙니다."));
        return;
    }
    if (FinalDamage <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageExecution - 양수 피해량이 전달되지 않았습니다."));
        return;
    }
    if (!TargetASC->GetSet<UBossAttributeSet>())
    {
        UE_LOG(LogTemp, Error, TEXT("BossDamageExecution - BossAttributeSet이 등록되지 않았습니다."));
        return;
    }

    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        UBossAttributeSet::GetHealthAttribute(),
        EGameplayModOp::Additive, -FinalDamage));
}