// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DoorConfigRow.generated.h"

class UCurveFloat;
/**
 * 
 */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FDoorConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // 문이 열리거나 닫히는 데 걸리는 시간
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Door",
        meta = (ClampMin = "0.01", Units = "s"))
    float MovementDuration = 1.0f;

    // 시간에 따른 이동 진행률
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Door")
    TObjectPtr<UCurveFloat> MovementCurve = nullptr;
};
