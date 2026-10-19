// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LaserSegment.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FLaserSegment
{
	GENERATED_BODY()

	// 이 레이저 구간의 시작 위치: 월드 좌표
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser")
	FVector StartPoint = FVector::ZeroVector;

	// 이 레이저 구간의 끝 위치: 월드 좌표
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser")
	FVector EndPoint = FVector::ZeroVector;
};