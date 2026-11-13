// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Gimmick/GimmickBase.h"
#include "BombDestructibleObstacle.generated.h"

class AExplosiveBomb;
class UStaticMeshComponent;
class UNiagaraSystem;
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

    virtual FProgressActorState CaptureProgress() const override;
    virtual void RestoreProgress(const FProgressActorState& State) override;

protected:
    // +추가----
    // 실제 파괴 시 한 번 재생하며 체크포인트 복구에는 재생하지 않는다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Audio")
    TObjectPtr<USoundBase> DestructionSound;
    // +--------

    // 통로를 막는 장애물 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> ObstacleMesh;

    // 장애물이 파괴될 때 한 번 재생할 Niagara
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|BombObstacle|Effects")
    TObjectPtr<UNiagaraSystem> DestructionEffect = nullptr;

    // 파괴 이펙트의 크기
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|BombObstacle|Effects", meta = (ClampMin = "0.01"))
    FVector DestructionEffectScale = FVector::OneVector;

    // 중복 파괴 방지
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BombObstacle")
    bool bIsDestroyed = false;

private:
    void ApplyDestroyedState(bool bDestroyed);

    ECollisionEnabled::Type IntactMeshCollision = ECollisionEnabled::NoCollision;
    bool bIntactMeshVisible = true;
    bool bIntactActorCollision = true;
    bool bIntactActorHidden = false;
};
