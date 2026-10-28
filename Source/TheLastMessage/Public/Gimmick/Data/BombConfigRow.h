// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Templates/SubclassOf.h"
#include "BombConfigRow.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
/**
 * 
 */
 // 폭탄의 폭발 및 재생성 설정
USTRUCT(BlueprintType)
struct THELASTMESSAGE_API FBombConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // 폭발 판정 반경
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb", meta = (ClampMin = "1.0", Units = "cm"))
    float ExplosionRadius = 300.0f;

    // 폭발 범위 안의 캐릭터에게 적용할 기본 피해량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb", meta = (ClampMin = "0.0"))
    float ExplosionDamage = 50.0f;

    // 폭발 후 새 폭탄을 생성하기까지의 대기 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb", meta = (ClampMin = "0.0", Units = "s"))
    float RespawnDelay = 3.0f;

    // 폭발 범위 안의 캐릭터에게 적용할 GAS 피해 효과
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb")
    TSubclassOf<UGameplayEffect> DamageEffectClass = nullptr;

    // 폭발 위치에 재생할 나이아가라 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb")
    TObjectPtr<UNiagaraSystem> ExplosionEffect = nullptr;
};
