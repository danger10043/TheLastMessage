#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "BossDamageLibrary.generated.h"

class UGameplayEffect;
class ABossMonster;

UCLASS()
class THELASTMESSAGE_API UBossDamageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 반환값은 GE 적용 성공 여부이며, 실제 체력 감소 여부가 아니다.
    UFUNCTION(BlueprintCallable, Category = "Boss|Damage")
    static bool ApplyFinalDamageToBoss(
        AActor* SourceActor, ABossMonster* Target,
        TSubclassOf<UGameplayEffect> DamageEffectClass,
        float FinalDamage, FGameplayTag DamageCategory);
};