#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "IncomingDamageExecution.generated.h"

UCLASS()
class THELASTMESSAGE_API UIncomingDamageExecution
    : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};