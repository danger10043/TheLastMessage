// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PushPullComponent.generated.h"

class APlayerBase;
class APushPullGimmick;
class UCharacterMovementComponent;
class UStaticMeshComponent;
class UPhysicsConstraintComponent;
//class UPhysicsHandleComponent;

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UPushPullComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPushPullComponent();

    // 물체 조작 시작
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    bool StartPushPull(APushPullGimmick* Target);

    // 이동 입력을 받아 물리 핸들의 목표 위치 갱신
    //FUNCTION(BlueprintCallable, Category = "Gimmick|PushPull")
    //void UpdatePushPull(const FVector& MoveDirection, float DeltaTime);

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

    float GetFollowSpeedX() const { return FollowSpeedX; }
    bool IsApproaching() const { return bApproaching; }
    bool GetHandContacts(FVector& Left, FVector& Right) const;

protected:
	virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 현재 조작 중인 물체
    UPROPERTY(Transient)
    TWeakObjectPtr<APushPullGimmick> CurrentTarget;

    // 물체를 잡고 움직일 물리 핸들
    //UPROPERTY(Transient)
    //TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

    // 플레이어에게 이동 평면 설정이 없을 때 사용할 법선
    // Y축을 법선으로 사용하면 X·Z 평면에서 이동
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PushPull")
    FVector MovementPlaneNormal = FVector(0.0f, 1.0f, 0.0f);

private:
    // 조작 중 독립 걷기를 막고, 해제할 때 복구할 속도를 보관
    void UpdateWalkSpeedLimit(UCharacterMovementComponent* Movement, float MoveSpeed);

    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerBase> OwnerPlayer;

    // 물리 핸들이 유지할 목표 위치와 회전
    //FVector TargetLocation = FVector::ZeroVector;
    //FRotator TargetRotation = FRotator::ZeroRotator;

    // 이번 조작에서 사용하는 이동 평면
    FVector ActivePlaneNormal = FVector(0.0f, 1.0f, 0.0f);

    // 조작 종료 시 복구할 걷기 속도
    float OriginalMaxWalkSpeed = 0.0f;

    // 컴포넌트가 마지막으로 적용한 제한 속도
    float AppliedMaxWalkSpeed = 0.0f;

    bool bHasWalkSpeedOverride = false;
    bool bOriginalOrientToMovement = false;

    // Follow the simulated box after physics, preserving the signed grab distance.
    double GrabOffsetX = 0.0;
    bool bOriginalPhysicsInteraction = true;
    bool bTargetWasMoveIgnored = false;
    void FollowSimulatedTarget(float DeltaTime);
    float FollowSpeedX = 0.0f;
    bool bApproaching = false;
    double ApproachStartX = 0.0;
    float ApproachElapsed = 0.0f;
    float ApproachDuration = 0.3f;
    double GripOffsetZ = 0.0;
    double GripHalfWidth = 14.0;
    UPROPERTY(Transient)
    TObjectPtr<UPhysicsConstraintComponent> HolderCollisionConstraint;

    // 낮은 턱을 넘을 수 있는지 검사하고, 가능하면 위치 보정
    bool TryStepUp(UStaticMeshComponent* Mesh, double DirectionSign, double ForwardDistance);
};
