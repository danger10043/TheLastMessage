// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "BombRespawnPoint.generated.h"

class AExplosiveBomb;
class USceneComponent;

// 폭탄의 최초 생성과 폭발 후 재생성을 관리하는 지점
UCLASS(Blueprintable)
class THELASTMESSAGE_API ABombRespawnPoint : public AActor
{
    GENERATED_BODY()

public:
    ABombRespawnPoint();

    // 기록된 위치에 폭탄 생성
    UFUNCTION(BlueprintCallable, Category = "Gimmick|BombRespawn")
    void SpawnBomb();

    // 대기 상태를 해제하고 새 폭탄 생성
    UFUNCTION(BlueprintCallable, Category = "Gimmick|BombRespawn")
    void RespawnBomb();

    // 자신이 생성한 폭탄의 폭발 알림 수신
    UFUNCTION(BlueprintCallable, Category = "Gimmick|BombRespawn")
    void HandleBombExploded(AExplosiveBomb* Bomb, float RespawnDelay);

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 레벨에서 생성 위치와 회전을 지정할 기준점
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    // 생성할 폭탄 Blueprint 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|BombRespawn")
    TSubclassOf<AExplosiveBomb> BombClass;

    // 게임 시작 시 기록한 생성 위치와 회전
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BombRespawn")
    FTransform SpawnTransform;

    // 현재 이 지점이 생성한 폭탄
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BombRespawn")
    TWeakObjectPtr<AExplosiveBomb> CurrentBomb;

private:
    // 재생성 대기 타이머
    FTimerHandle RespawnTimerHandle;

    // 중복 재생성 요청 방지
    bool bRespawnPending = false;
};
