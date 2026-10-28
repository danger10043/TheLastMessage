// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "FuseCircuitSocket.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class AFuseItem;
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

    UFUNCTION(BlueprintCallable, Category = "Gimmick|Fuse")
    bool TryInsertFuse(ACharacter* Player);

protected:
    // 회로의 외형
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> CircuitMesh;

    // 퓨즈를 장착할 위치와 회전의 기준
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<USceneComponent> FuseMountPoint;

    // 현재 회로에 장착된 실제 퓨즈 액터
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Fuse")
    TObjectPtr<AFuseItem> InstalledFuse;

private:
    // 삽입 처리 도중 중복 요청 방지
    bool bIsInserting = false;

};
