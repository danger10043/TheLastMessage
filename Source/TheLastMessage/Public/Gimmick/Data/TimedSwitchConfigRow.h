// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TimedSwitchConfigRow.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FTimedSwitchConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// 첫 스위치를 켠 순간부터 적용되는 제한 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimedSwitch", meta = (ClampMin = "0.01", Units = "s"))
	float TimeLimit = 10.0f;

	// 남은 시간이 이 값 이하이면 빠르게 깜빡임
	// 0이면 빠른 깜빡임 전환을 사용하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimedSwitch", meta = (ClampMin = "0.0", Units = "s"))
	float FastBlinkThreshold = 3.0f;
};
