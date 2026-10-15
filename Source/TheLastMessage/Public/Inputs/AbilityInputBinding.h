#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilityInputBinding.generated.h"

class UInputAction;

/** Enhanced Input 액션과 부여된 GAS 능력의 입력 태그를 연결한다. */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FAbilityInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	FGameplayTag InputTag;
};
