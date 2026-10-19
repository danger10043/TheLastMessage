#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "CombatDamageLibrary.generated.h"

class UGameplayEffect;
struct FGameplayEffectSpec;

UCLASS()
class THELASTMESSAGE_API UCombatDamageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // True means the GE was accepted, not necessarily that Health decreased.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Damage")
    static bool ApplyFinalDamageToTarget(
        AActor* SourceActor, AActor* Target,
        TSubclassOf<UGameplayEffect> DamageEffectClass,
        float FinalDamage, FGameplayTag DamageCategory, float FeedbackStrength = 1.0f);

    // Rejects missing or ambiguous damage categories.
    static bool TryGetDamageCategory(const FGameplayEffectSpec& Spec, FGameplayTag& OutCategory);
};
