// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Carry/CarryableGimmick.h"
#include "FuseItem.generated.h"

/**
 * 
 */
 // 플레이어가 운반하고 퓨즈 회로에 삽입할 수 있는 아이템
UCLASS(Blueprintable)
class THELASTMESSAGE_API AFuseItem : public ACarryableGimmick
{
    GENERATED_BODY()

public:
    // 장착된 퓨즈는 다시 집을 수 없음
    virtual bool CanBePickedUp(ACharacter* Player) const override;

    // 회로로 전달되면 장착 상태로 전환
    virtual void OnTransferred() override;

    // +추가----
    // 체크포인트 복원 시 퓨즈의 장착·상호작용 상태 초기화
    virtual void RestoreProgress(const FProgressActorState& State) override;
    // +--------

    UFUNCTION(BlueprintPure, Category = "Gimmick|Fuse")
    bool IsInstalled() const
    {
        return bIsInstalled;
    }

protected:
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Fuse")
    bool bIsInstalled = false;
};
