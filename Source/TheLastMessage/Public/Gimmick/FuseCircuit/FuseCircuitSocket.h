// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "FuseCircuitSocket.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class AFuseItem;
class UMaterialInstanceDynamic;
/**
 * 
 */
 // 퓨즈를 삽입해서 연결된 장치를 활성화하는 회로
UCLASS(Blueprintable)
class THELASTMESSAGE_API AFuseCircuitSocket : public AGimmickBase
{
	GENERATED_BODY()
	
public:
    virtual FProgressActorState CaptureProgress() const override;
    virtual void FinishRestoreProgress(const FProgressActorState& State) override;
    virtual int32 GetRestorePriority() const override { return 2; }
    AFuseCircuitSocket();

    // 현재 플레이어가 퓨즈를 삽입할 수 있는지 확인
    virtual bool CanInteractInternal(ACharacter* Player) const override;

    // 상호작용 요청으로 퓨즈 삽입 시도
    virtual void PerformInteraction(ACharacter* Player) override;

    // 활성 상태가 바뀌면 표시등도 갱신
    virtual void ApplyActiveState(bool bNewActive) override;

    UFUNCTION(BlueprintCallable, Category = "Gimmick|Fuse")
    bool TryInsertFuse(ACharacter* Player);

protected:
    virtual void BeginPlay() override;

    // 회로의 외형
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> CircuitMesh;

    // 퓨즈를 장착할 위치와 회전의 기준
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<USceneComponent> FuseMountPoint;

    // 현재 회로에 장착된 실제 퓨즈 액터
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Fuse")
    TObjectPtr<AFuseItem> InstalledFuse;

    // CircuitMesh에서 표시등 재질이 들어 있는 슬롯 번호
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Fuse|Appearance", meta = (ClampMin = "0"))
    int32 IndicatorMaterialIndex = 1;

private:
    // 삽입 처리 도중 중복 요청 방지
    bool bIsInserting = false;

    void UpdateCircuitIndicator();

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CircuitIndicatorMaterial;

};
