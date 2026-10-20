// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Templates/SubclassOf.h"
#include "CarryConfigRow.generated.h"

class UGameplayEffect;
/**
 * 
 */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FCarryConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // 플레이어의 운반 부착 지점을 기준으로 적용할 위치와 회전
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    FTransform CarryOffset = FTransform::Identity;

    // 플레이어 앞쪽에 물체를 내려놓을 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (ClampMin = "1.0", Units = "cm"))
    float DropDistance = 150.0f;

    // 물체를 운반하는 동안 플레이어에게 적용할 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry")
    TSubclassOf<UGameplayEffect> CarryEffectClass = nullptr;

    // 운반 중 점프 초기 속도 배율
    // 1이면 기존 점프 속도 유지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float JumpVelocityMultiplier = 1.0f;
};