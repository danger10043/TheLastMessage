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
};