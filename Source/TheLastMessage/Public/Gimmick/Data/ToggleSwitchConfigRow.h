// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "ToggleSwitchConfigRow.generated.h"

class UCurveFloat;
/**
 * 
 */
// 스위치 레버의 회전 연출 설정
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FToggleSwitchConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // 꺼짐 각도에서 켜짐 각도까지 움직이는 데 걸리는 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleSwitch", meta = (ClampMin = "0.01", Units = "s"))
    float RotationDuration = 0.3f;

    // 시간에 따른 회전 진행도
    // 커브의 시간과 값은 모두 0~1 범위로 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleSwitch")
    TObjectPtr<UCurveFloat> LeverRotationCurve = nullptr;

    // 꺼진 상태의 레버 각도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleSwitch")
    float LeverOffPitch = -45.0f;

    // 켜진 상태의 레버 각도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToggleSwitch")
    float LeverOnPitch = 45.0f;
};