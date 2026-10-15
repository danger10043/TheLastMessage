#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "BossDamageExecution.generated.h"

// 보스 전용 최종 피해 전달용. 공통 피해 산식을 구현하는 클래스가 아니다.
UCLASS()
class THELASTMESSAGE_API UBossDamageExecution
    : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};