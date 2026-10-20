// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/CarryConfigRow.h"
#include "CarryableGimmick.generated.h"

class ACharacter;
class UStaticMeshComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ACarryableGimmick : public AGimmickBase
{
	GENERATED_BODY()
	
public:
    ACarryableGimmick();

    // 데이터 테이블에서 운반 설정 읽기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Carry")
    bool LoadCarryConfig();

    // 현재 이 물체를 집을 수 있는지 확인
    UFUNCTION(BlueprintPure, Category = "Gimmick|Carry")
    virtual bool CanBePickedUp(ACharacter* Player) const;

    // BPI CanInteract에서 사용하는 조건
    virtual bool CanInteractInternal(ACharacter* Player) const override;

    // CarryComponent가 집기 완료 시 호출
    virtual void OnPickedUp(ACharacter* Player);

    // CarryComponent가 운반 종료 시 호출
    virtual void OnDropped();

    // CarryComponent에서 사용하는 접근 함수
    UStaticMeshComponent* GetObjectMesh() const;
    ACharacter* GetCarrier() const;

    const FCarryConfigRow& GetCarryConfig() const
    {
        return CarryConfig;
    }

protected:
    virtual void BeginPlay() override;

    // 운반할 물체의 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> ObjectMesh;

    // 현재 이 물체를 들고 있는 캐릭터
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Carry")
    TWeakObjectPtr<ACharacter> Carrier;

    // 사용할 운반 데이터 테이블과 행
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Carry")
    FDataTableRowHandle CarryConfigRow;

    // 데이터 테이블에서 읽은 설정
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Carry")
    FCarryConfigRow CarryConfig;

private:
    bool bCarryConfigLoaded = false;
};
