#include "Character/CombatDamageLibrary.h"
#include "Character/IncomingDamageExecution.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "Character/Player/CombatFeedbackComponent.h"
#include "Character/Enemy/EnemyAttributeSet.h"
#include "Character/Boss/BossAttributeSet.h"

namespace CombatGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG(Event_DamageDealt, "Event.Combat.DamageDealt");
    UE_DEFINE_GAMEPLAY_TAG(Result_TargetKilled, "Combat.Result.TargetKilled");
}

namespace
{
    bool IsSupportedDamageCategory(const FGameplayTag& Tag)
    {
        const FName Name = Tag.GetTagName();
        return Tag.IsValid()
            && (Name == FName(TEXT("Damage.Category.Player.Melee"))
                || Name == FName(TEXT("Damage.Category.Player.Ranged"))
                || Name == FName(TEXT("Damage.Category.Gimmick")));
    }
}

bool UCombatDamageLibrary::TryGetDamageCategory(
    const FGameplayEffectSpec& Spec, FGameplayTag& OutCategory)
{
    OutCategory = FGameplayTag();
    for (const FGameplayTag& Tag : Spec.GetDynamicAssetTags())
    {
        if (!IsSupportedDamageCategory(Tag))
        {
            continue;
        }
        if (OutCategory.IsValid())
        {
            OutCategory = FGameplayTag();
            return false;
        }
        OutCategory = Tag;
    }
    return OutCategory.IsValid();
}

bool UCombatDamageLibrary::ApplyFinalDamageToTarget(
    AActor* SourceActor, AActor* Target,
    TSubclassOf<UGameplayEffect> DamageEffectClass,
    float FinalDamage, FGameplayTag DamageCategory, float FeedbackStrength)
{
    if (!IsValid(SourceActor) || !IsValid(Target)
        || !SourceActor->HasAuthority() || !Target->HasAuthority()
        || !FMath::IsFinite(FinalDamage) || FinalDamage <= 0.f
        || !IsSupportedDamageCategory(DamageCategory) || !DamageEffectClass)
    {
        return false;
    }

    UAbilitySystemComponent* SourceASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
    UAbilitySystemComponent* TargetASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!IsValid(SourceASC) || !IsValid(TargetASC))
    {
        return false;
    }

    const UGameplayEffect* Effect = DamageEffectClass.GetDefaultObject();
    if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Instant
        || Effect->Modifiers.Num() != 0 || Effect->Executions.Num() != 1
        || Effect->Executions[0].CalculationClass
        != UIncomingDamageExecution::StaticClass())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CombatDamage: use an Instant GE with no Modifiers and one IncomingDamageExecution."));
        return false;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Data.Damage")), false);
    if (!DamageTag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatDamage: Data.Damage tag is missing."));
        return false;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(SourceActor);
    FGameplayEffectSpecHandle Spec =
        SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
    if (!Spec.IsValid())
    {
        return false;
    }

    Spec.Data->SetSetByCallerMagnitude(DamageTag, FinalDamage);
    Spec.Data->AddDynamicAssetTag(DamageCategory);
    const FGameplayAttribute HealthAttribute = TargetASC->GetSet<UBossAttributeSet>()
        ? UBossAttributeSet::GetHealthAttribute() : UEnemyAttributeSet::GetHealthAttribute();
    const bool bHasHealth = TargetASC->HasAttributeSetForAttribute(HealthAttribute);
    const float HealthBefore = bHasHealth ? TargetASC->GetNumericAttribute(HealthAttribute) : 0.0f;
    const FActiveGameplayEffectHandle Applied =
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    const float HealthAfter = bHasHealth && IsValid(TargetASC)
        ? TargetASC->GetNumericAttribute(HealthAttribute) : HealthBefore;
    const float ActualDamage = HealthBefore - HealthAfter;
    // 결과를 먼저 확정하여 피드백이나 이벤트 수신 중 체력이 바뀌어도 이번 공격의 판정을 유지한다.
    const bool bTargetKilled = HealthBefore > 0.f && HealthAfter <= 0.f;
    // 면역·사망·페이즈 제한을 반영한 체력 감소가 있을 때만 플레이어 공격 결과를 전달한다.
    if (FMath::IsFinite(ActualDamage) && ActualDamage > 0.0f
        && DamageCategory.GetTagName() != FName(TEXT("Damage.Category.Gimmick")) && IsValid(SourceActor))
    {
        if (UCombatFeedbackComponent* Feedback = SourceActor->FindComponentByClass<UCombatFeedbackComponent>())
            Feedback->NotifyDamageDealt(FeedbackStrength);

        if (IsValid(SourceActor) && IsValid(SourceASC))
        {
            FGameplayEventData Payload;
            Payload.EventTag = CombatGameplayTags::Event_DamageDealt;
            Payload.Instigator = SourceActor;
            Payload.Target = Target;
            Payload.EventMagnitude = ActualDamage;
            Payload.InstigatorTags.AddTag(DamageCategory);
            Payload.ContextHandle = Context;
            if (bTargetKilled)
                Payload.TargetTags.AddTag(CombatGameplayTags::Result_TargetKilled);
            SourceASC->HandleGameplayEvent(Payload.EventTag, &Payload);
        }
    }
    return Applied.WasSuccessfullyApplied();
}
