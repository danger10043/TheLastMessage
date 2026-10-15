#include "Character/Boss/BossDamageLibrary.h"
#include "Character/Boss/BossMonster.h"
#include "Character/Boss/BossDamageExecution.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

bool UBossDamageLibrary::ApplyFinalDamageToBoss(
    AActor* SourceActor, ABossMonster* Target,
    TSubclassOf<UGameplayEffect> DamageEffectClass,
    float FinalDamage, FGameplayTag DamageCategory)
{
    if (!IsValid(SourceActor))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 공격자가 유효하지 않습니다."));
        return false;
    }
    if (!IsValid(Target))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 대상 보스가 유효하지 않습니다."));
        return false;
    }
    if (!SourceActor->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossDamageLibrary - 공격자에 실행 권한이 없습니다."));
        return false;
    }
    if (!Target->HasAuthority())
    {
        UE_LOG(LogTemp, Verbose, TEXT("BossDamageLibrary - 대상 보스에 실행 권한이 없습니다."));
        return false;
    }
    if (!FMath::IsFinite(FinalDamage))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 피해량이 유한한 수가 아닙니다."));
        return false;
    }
    if (FinalDamage <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 피해량은 0보다 커야 합니다."));
        return false;
    }

    const FName AllowedCategories[] = {
        FName(TEXT("Damage.Category.Gimmick")),
        FName(TEXT("Damage.Category.Player.Melee")),
        FName(TEXT("Damage.Category.Player.Ranged"))
    };
    bool bSupportedCategory = false;
    for (const FName CategoryName : AllowedCategories)
    {
        if (DamageCategory.GetTagName() == CategoryName)
        {
            bSupportedCategory = true;
            break;
        }
    }
    if (!bSupportedCategory)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 지원하지 않는 피해 분류입니다: %s"),
            *DamageCategory.ToString());
        return false;
    }

    UAbilitySystemComponent* SourceASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
    if (!IsValid(SourceASC))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 공격자의 ASC가 없습니다."));
        return false;
    }
    UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
    if (!IsValid(TargetASC))
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 보스의 ASC가 없습니다."));
        return false;
    }
    if (!DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 피해 GE가 지정되지 않았습니다."));
        return false;
    }

    const UGameplayEffect* Effect = DamageEffectClass.GetDefaultObject();
    if (Effect->DurationPolicy != EGameplayEffectDurationType::Instant)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 피해 GE는 Instant여야 합니다."));
        return false;
    }
    if (Effect->Modifiers.Num() != 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 피해 GE의 Modifiers는 비워야 합니다."));
        return false;
    }
    if (Effect->Executions.Num() != 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - Execution은 하나만 지정해야 합니다."));
        return false;
    }
    if (Effect->Executions[0].CalculationClass
        != UBossDamageExecution::StaticClass())
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - BossDamageExecution을 지정해야 합니다."));
        return false;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Data.Damage")), false);
    if (!DamageTag.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BossDamageLibrary - Data.Damage 태그가 등록되지 않았습니다."));
        return false;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(SourceActor);
    FGameplayEffectSpecHandle Spec =
        SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
    if (!Spec.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BossDamageLibrary - 피해 Spec 생성에 실패했습니다."));
        return false;
    }

    Spec.Data->SetSetByCallerMagnitude(DamageTag, FinalDamage);
    Spec.Data->AddDynamicAssetTag(DamageCategory);

    const FActiveGameplayEffectHandle Applied =
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    if (!Applied.WasSuccessfullyApplied())
    {
        UE_LOG(LogTemp, Warning, TEXT("BossDamageLibrary - 피해 GE 적용이 거부되었습니다."));
        return false;
    }
    return true;
}