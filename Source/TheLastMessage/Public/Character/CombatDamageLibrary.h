#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "CombatDamageLibrary.generated.h"

class UGameplayEffect;
struct FGameplayEffectSpec;

namespace CombatGameplayTags
{
    // 공격자 ASC의 피해 결과 알림. EventMagnitude는 실제 체력 감소량,
    // InstigatorTags는 근접/원거리 피해 분류이며 Instigator와 Target은 공격자와 피격 대상이다.
    THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_DamageDealt);
}

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
