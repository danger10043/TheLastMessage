// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AirflowConfigRow.generated.h"

class UNiagaraSystem;
/**
 * 
 */
 // 환풍기의 기류 설정을 담는 데이터 테이블 행
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FAirflowConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	// 환풍기 기준 바람 방향
	// (0, 0, 1)은 환풍기의 로컬 위쪽 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airflow")
	FVector LocalWindDirection = FVector::UpVector;

	// 바람에 의해 추가되는 가속도
	// 단위: cm/s²
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airflow", meta = (ClampMin = "0.0"))
	float WindAcceleration = 3500.0f;

	// 기류 안에서 허용할 최대 상승 속도
	// 상승 방향은 플레이어의 현재 중력 반대 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airflow", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxRiseSpeed = 600.0f;

	// 기류 안에서 허용할 최대 낙하 속도
	// 낙하 방향은 플레이어의 현재 중력 방향
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airflow", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxFallSpeed = 350.0f;

	// 기류를 표시할 Niagara 시스템
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airflow")
	TObjectPtr<UNiagaraSystem> AirflowEffect = nullptr;
};
