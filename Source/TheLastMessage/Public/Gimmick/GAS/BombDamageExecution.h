// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/IncomingDamageExecution.h"
#include "BombDamageExecution.generated.h"

/**
 * 
 */
 // 폭탄 피해를 대상 캐릭터의 피해 속성으로 전달
UCLASS()
class THELASTMESSAGE_API UBombDamageExecution : public UIncomingDamageExecution
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& ExecutionParams,
        FGameplayEffectCustomExecutionOutput& OutExecutionOutput
    ) const override;
};
