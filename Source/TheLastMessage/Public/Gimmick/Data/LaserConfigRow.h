// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LaserConfigRow.generated.h"

class UNiagaraSystem;
/**
 * 
 */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FLaserConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// 레이저의 최대 진행 거리
	// 언리얼의 기본 거리 단위는 cm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxLaserDistance = 3000.0f;

	// 레이저를 표시할 Niagara 시스템
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
	TObjectPtr<UNiagaraSystem> LaserEffect = nullptr;

	// 플레이어 캡슐 반지름에 곱할 사망 판정 비율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|PlayerHit", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PlayerHitRadiusScale = 1.0f;

	// 플레이어 캡슐 반높이에 곱할 사망 판정 비율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|PlayerHit", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PlayerHitHeightScale = 1.0f;
	
};
