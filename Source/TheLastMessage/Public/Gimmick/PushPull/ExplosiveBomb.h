// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/PushPull/PushPullGimmick.h"
#include "Gimmick/Data/BombConfigRow.h"
#include "ExplosiveBomb.generated.h"

class ABombRespawnPoint;
/**
 * 
 */
 // 밀고 당길 수 있으며 공격받으면 폭발하는 폭탄
UCLASS(Blueprintable)
class THELASTMESSAGE_API AExplosiveBomb : public APushPullGimmick
{
    GENERATED_BODY()

public:
    // 데이터 테이블에서 폭탄 설정 읽기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Bomb")
    bool LoadBombConfig();

    // 폭발 여부와 설정 상태를 포함한 조작 가능 여부 검사
    virtual bool CanBeMovedBy(ACharacter* Player) const override;

    // 공격이 명중했을 때 무기 쪽에서 호출
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Bomb")
    void ReceiveAttackDamage(float DamageAmount, AActor* DamageCauser);

    // 폭발 실행
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Bomb")
    void Explode();

    // 이 폭탄을 생성한 재생성 지점 등록
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Bomb")
    void SetRespawnPoint(ABombRespawnPoint* InRespawnPoint);

    // 이미 폭발했는지 확인
    UFUNCTION(BlueprintPure, Category = "Gimmick|Bomb")
    bool HasExploded() const
    {
        return bHasExploded;
    }

    const FBombConfigRow& GetBombConfig() const
    {
        return BombConfig;
    }

protected:
    virtual void BeginPlay() override;

    // 폭발 범위에서 찾은 액터에 피해 또는 장애물 파괴 적용
    void ApplyExplosionToActor(AActor* Target);

    // 사용할 폭탄 데이터 테이블과 행
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
    FDataTableRowHandle BombConfigRow;

    // 데이터 테이블에서 읽은 폭탄 설정
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Bomb")
    FBombConfigRow BombConfig;

    // 중복 폭발 방지
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Bomb")
    bool bHasExploded = false;

    // 폭발 후 새 폭탄을 생성할 지점
    UPROPERTY(Transient)
    TWeakObjectPtr<ABombRespawnPoint> RespawnPoint;

private:
    // 폭탄 설정을 정상적으로 읽었는지 확인
    bool bHasValidBombConfig = false;
};