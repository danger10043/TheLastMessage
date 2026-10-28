// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "BombDestructibleObstacle.generated.h"

class AExplosiveBomb;
class UStaticMeshComponent;
/**
 * 
 */
 // 폭탄 폭발로 파괴되어 통로를 개방하는 장애물
UCLASS(Blueprintable)
class THELASTMESSAGE_API ABombDestructibleObstacle : public AGimmickBase
{
    GENERATED_BODY()

public:
    ABombDestructibleObstacle();

    // 폭탄이 폭발 범위 안의 장애물에 호출
    UFUNCTION(BlueprintCallable, Category = "Gimmick|BombObstacle")
    void ReceiveBombExplosion(AExplosiveBomb* Bomb);

    // 파괴 상태 기록 및 외형과 충돌 제거
    UFUNCTION(BlueprintCallable, Category = "Gimmick|BombObstacle")
    void DestroyObstacle();

protected:
    // 통로를 막는 장애물 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> ObstacleMesh;

    // 중복 파괴 방지
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BombObstacle")
    bool bIsDestroyed = false;
};
