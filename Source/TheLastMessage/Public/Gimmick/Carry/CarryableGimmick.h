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
    virtual FProgressActorState CaptureProgress() const override;
    virtual void RestoreProgress(const FProgressActorState& State) override;
    ACarryableGimmick();

    // 데이터 테이블에서 운반 설정 읽기
    UFUNCTION(BlueprintCallable, Category = "Gimmick|Carry")
    bool LoadCarryConfig();

    // 현재 이 물체를 집을 수 있는지 확인
    UFUNCTION(BlueprintPure, Category = "Gimmick|Carry")
    virtual bool CanBePickedUp(ACharacter* Player) const;

    // BPI CanInteract에서 사용하는 조건
    virtual bool CanInteractInternal(ACharacter* Player) const override;

    // 상호작용 요청을 플레이어의 운반 컴포넌트에 전달
    virtual void PerformInteraction(ACharacter* Player) override;

    // CarryComponent가 집기 완료 시 호출
    virtual void OnPickedUp(ACharacter* Player);

    // CarryComponent가 운반 종료 시 호출
    virtual void OnDropped();

    // 다른 장착 지점으로 전달되어 운반이 끝났을 때 호출
    virtual void OnTransferred();

    // 내려놓을 위치를 아이템의 최초 Y 좌표에 맞게 보정
    virtual FVector ConstrainDropLocation(const FVector& WorldLocation) const;

    // CarryComponent에서 사용하는 접근 함수
    UStaticMeshComponent* GetObjectMesh() const;
    ACharacter* GetCarrier() const;

    virtual bool UsesWideCarryPose() const { return false; }
    bool ReserveForPickup(ACharacter* Player);
    void ReleasePickupReservation(ACharacter* Player);

    const FCarryConfigRow& GetCarryConfig() const
    {
        return CarryConfig;
    }

    // 게임 시작 시 최초 배치 상태 반환
    UFUNCTION(BlueprintPure, Category = "Gimmick|Checkpoint")
    FTransform GetInitialWorldTransform() const
    {
        return InitialWorldTransform;
    }

    // 마지막으로 기록한 배치 상태 반환
    UFUNCTION(BlueprintPure, Category = "Gimmick|Checkpoint")
    FTransform GetLastPlacedWorldTransform() const
    {
        return LastPlacedWorldTransform;
    }

protected:
    virtual void BeginPlay() override;

    virtual void Tick(float DeltaSeconds) override;

    // 운반할 물체의 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> ObjectMesh;

    // 현재 이 물체를 들고 있는 캐릭터
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Carry")
    TWeakObjectPtr<ACharacter> Carrier;

    // 사용할 운반 데이터 테이블과 행
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
    FDataTableRowHandle CarryConfigRow;

    // 데이터 테이블에서 읽은 설정
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Carry")
    FCarryConfigRow CarryConfig;

    // 게임 시작 시 최초 배치 상태
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Checkpoint")
    FTransform InitialWorldTransform = FTransform::Identity;

    // 상호작용으로 이동한 뒤의 월드 배치 상태
    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Checkpoint")
    FTransform LastPlacedWorldTransform = FTransform::Identity;

private:
    TWeakObjectPtr<ACharacter> ReservedCarrier;
    bool bCarryConfigLoaded = false;

    // 물리 시뮬레이션의 Y축 이동 제한 적용
    void ApplyYLock();

    // 현재 위치를 최초 Y 좌표에 맞게 보정
    void UpdateYLockedLocation();

    // 게임 시작 시 기록한 월드 Y 좌표
    double InitialWorldY = 0.0;

    // 최초 Y 좌표 기록 여부
    bool bHasInitialWorldY = false;

    // 내려놓은 아이템의 수직 낙하 갱신
    void UpdateDropFall(float DeltaSeconds);

    // 현재 내려놓기로 인한 낙하 중인지
    bool bIsDropFalling = false;

    // 현재 아래쪽으로 떨어지는 속도
    float DropFallSpeed = 0.0f;

    // 낙하 검사에서 방금 아이템을 내려놓은 플레이어 제외
    TWeakObjectPtr<ACharacter> DropIgnoredPlayer;

};
