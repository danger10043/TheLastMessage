// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PushPullComponent.generated.h"

class APlayerBase;
class APushPullGimmick;
class UCharacterMovementComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UPushPullComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPushPullComponent();

    // 물체 조작 시작
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    bool StartPushPull(APushPullGimmick* Target);

    // 이동 입력에 따라 물체의 수평 속도를 제어
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    void UpdatePushPull(const FVector& MoveDirection, float DeltaTime);

    // 물리 제어 해제 및 상태 복구
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    void StopPushPull();

    UFUNCTION(BlueprintPure, Category = "Gimmick|PushPull")
    bool IsPushPulling() const;

    UFUNCTION(BlueprintPure, Category = "Gimmick|PushPull")
    APushPullGimmick* GetCurrentTarget() const;

    // 플레이어가 입력한 좌우 이동값 전달
    void SetMoveInput(float Value);

protected:
	virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 현재 조작 중인 물체
    UPROPERTY(Transient)
    TWeakObjectPtr<APushPullGimmick> CurrentTarget;

    // 플레이어에게 이동 평면 설정이 없을 때 사용할 법선
    // Y축을 법선으로 사용하면 X·Z 평면에서 이동
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PushPull")
    FVector MovementPlaneNormal = FVector(0.0f, 1.0f, 0.0f);

private:
    // 조작 중 걷기 속도를 물체의 이동 속도에 맞춰 제한
    void UpdateWalkSpeedLimit(UCharacterMovementComponent* Movement, float MoveSpeed);

    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerBase> OwnerPlayer;

    // 이번 조작에서 사용하는 이동 평면
    FVector ActivePlaneNormal = FVector(0.0f, 1.0f, 0.0f);

    // 조작 종료 시 복구할 걷기 속도
    float OriginalMaxWalkSpeed = 0.0f;

    // 컴포넌트가 마지막으로 적용한 제한 속도
    float AppliedMaxWalkSpeed = 0.0f;

    bool bHasWalkSpeedOverride = false;
    bool bOriginalOrientToMovement = false;

    // 낮은 턱을 넘을 수 있는지 검사하고, 가능하면 위치 보정
    bool TryStepUp(UStaticMeshComponent* Mesh, double DirectionSign, double ForwardDistance);

    // 공통 이동에 사용할 좌우 입력
    float PendingMoveInput = 0.0f;

    // 조작 시작 시 플레이어와 물체 사이의 X축 위치 차이
    double InitialRelativeOffsetX = 0.0;

    // 플레이어와 물체에 함께 사용할 수평 이동 속도
    double SharedMoveSpeedX = 0.0;

   
    // 실제 충돌 형상으로 이동 경로를 검사하고
    // 이동을 막는 가장 가까운 충돌 결과 반환
    bool SweepBlockingMovement(UPrimitiveComponent* Component, const FVector& Start, const FVector& End, FHitResult& OutHit) const;

    // 지정한 수평 이동이 가능한지 검사하고 도착 위치 계산
    // 도착 위치에 받쳐 주는 바닥이 있는지도 함께 반환
    bool FindSharedMoveLocation(UPrimitiveComponent* Component, double DeltaX, double MaxStepHeight, double MaxSlopeAngle, FVector& OutLocation, bool& bOutOnGround) const;

    // 공통 이동을 위해 자동 이동 상태 전환
    bool BeginSharedMovement(UStaticMeshComponent* Mesh);

    // 조작 종료 시 플레이어의 일반 이동 복구
    void EndSharedMovement();

    // 공통 이동 전환이 적용되어 있는지 확인
    bool bSharedMovementActive = false;

    // 전환 전 캐릭터 무브먼트의 갱신 상태
    bool bSavedMovementTickEnabled = false;
};
