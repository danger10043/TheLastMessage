#include "Character/CombatDamageLibrary.h"
#include "Character/IncomingDamageExecution.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

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
    float FinalDamage, FGameplayTag DamageCategory)
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
    const FActiveGameplayEffectHandle Applied =
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    return Applied.WasSuccessfullyApplied();
}