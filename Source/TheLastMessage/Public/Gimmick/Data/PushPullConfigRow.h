// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PushPullConfigRow.generated.h"

/**
 * 
 */
 // 밀기·당기기에 사용하는 데이터 테이블 행
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FPushPullConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // 물체를 밀거나 당길 때의 이동 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushPull", meta = (ClampMin = "1.0", Units = "cm/s"))
    float MoveSpeed = 200.0f;

    // 플레이어와 물체 사이의 조작 유지 가능 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushPull", meta = (ClampMin = "1.0", Units = "cm"))
    float MaxSeparationDistance = 250.0f;

    // 목표 수평 속도에 도달하거나 멈출 때 사용할 최대 가속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushPull", meta = (ClampMin = "1.0"))
    float HorizontalAcceleration = 2000.0f;

    // 바닥으로 취급하여 이동을 허용할 최대 경사각
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushPull", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
    float MaxSlopeAngle = 40.0f;

    // 조작 중 넘을 수 있는 최대 턱 높이
// 0이면 턱 넘기 기능 사용 안 함
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PushPull", meta = (ClampMin = "0.0", Units = "cm"))
    float MaxStepHeight = 20.0f;
};