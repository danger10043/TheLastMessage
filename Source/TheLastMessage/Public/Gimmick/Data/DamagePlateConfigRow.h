// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "DamagePlateConfigRow.generated.h"

class UGameplayEffect;
/**
 * 
 */
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FDamagePlateConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // 한 번 적용할 기본 피해량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamagePlate", meta = (ClampMin = "0.0"))
    float DamageAmount = 10.0f;

    // 피해를 반복 적용하는 간격
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamagePlate", meta = (ClampMin = "0.01", Units = "s"))
    float DamageInterval = 1.0f;

    // 플레이어에게 적용할 피해 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamagePlate")
    TSubclassOf<UGameplayEffect> DamageEffectClass = nullptr;

    // 플레이어가 이 태그를 가지고 있으면 피해를 받지 않음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamagePlate", meta = (Categories = "State.Carrying"))
    FGameplayTag ProtectionTag;
};
