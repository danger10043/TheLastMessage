// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/PushPullConfigRow.h"
#include "PushPullGimmick.generated.h"

class ACharacter;
class USceneComponent;
class UStaticMeshComponent;
/**
 * 
 */
 // 무게 상자와 폭탄이 공통으로 사용하는 밀기·당기기 부모
UCLASS(Blueprintable)
class THELASTMESSAGE_API APushPullGimmick : public AGimmickBase
{
    GENERATED_BODY()

public:
    APushPullGimmick();

    // 데이터 테이블에서 이동 설정 읽기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    bool LoadPushPullConfig();

    // 해당 플레이어가 이 물체의 조작을 시작할 수 있는지 확인
    UFUNCTION(BlueprintPure, Category = "Gimmick|PushPull")
    virtual bool CanBeMovedBy(ACharacter* Player) const;

    // BPI CanInteract에서 사용하는 조건
    virtual bool CanInteractInternal(ACharacter* Player) const override;

    // BPI Interact에서 호출하여 밀기·당기기 시작 요청
    virtual void PerformInteraction(ACharacter* Player) override;

    // 조작 중인 플레이어 기록
    // 실제 물리 제어는 PushPullComponent에서 처리
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    virtual void BeginPushPull(ACharacter* Player);

    // 조작 중인 플레이어 참조 정리
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    virtual void EndPushPull();

    UStaticMeshComponent* GetPhysicsMesh() const;
    USceneComponent* GetInteractionAnchor() const;
    ACharacter* GetControllingPlayer() const;

    const FPushPullConfigRow& GetPushPullConfig() const
    {
        return PushPullConfig;
    }

protected:
    virtual void BeginPlay() override;

    // 실제로 움직일 물체의 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> PhysicsMesh;

    // 플레이어가 조작할 때 사용하는 기준 지점
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<USceneComponent> InteractionAnchor;

    // 현재 물체를 조작하는 플레이어
    UPROPERTY(Transient)
    TWeakObjectPtr<ACharacter> ControllingPlayer;

    // 물체의 조작 허용 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PushPull")
    bool bCanBeMoved = true;

    // 사용할 데이터 테이블과 행
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
    FDataTableRowHandle PushPullConfigRow;

    // 데이터 테이블에서 읽은 설정
    UPROPERTY(Transient)
    FPushPullConfigRow PushPullConfig;

private:
    // 설정이 정상적으로 로딩됐는지 확인
    bool bHasValidPushPullConfig = false;

    // 조작 여부에 따라 수평 이동 잠금 상태 변경
    void SetPushPullMovementEnabled(bool bAllowHorizontalMovement);
};
