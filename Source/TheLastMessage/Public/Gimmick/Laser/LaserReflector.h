// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Interface/AttackReceiverInterface.h"
#include "LaserReflector.generated.h"

class ACharacter;
class USceneComponent;
class UStaticMeshComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ALaserReflector : public AGimmickBase, public IAttackReceiverInterface
{
	GENERATED_BODY()
	
public:
    virtual FProgressActorState CaptureProgress() const override;
    virtual void RestoreProgress(const FProgressActorState& State) override;
	ALaserReflector();

	// 플레이어 상호작용으로 반사판 회전
	virtual void PerformInteraction(ACharacter* Player) override;

	// 플레이어 공격을 받으면 반사판 회전
	virtual void ReceiveAttackDamage(float DamageAmount, AActor* DamageCauser) override;

	// 지정한 로컬 축으로 설정된 각도만큼 회전
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Laser")
	void RotateReflector();

	// 플레이 시작 시의 반사판 각도로 복구
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Laser")
	void ResetReflector();

	// 입사 방향과 표면 법선으로 반사 방향 계산
	UFUNCTION(BlueprintPure, Category = "Gimmick|Laser")
	FVector GetReflectedDirection(const FVector& IncomingDirection, const FVector& SurfaceNormal) const;

protected:
	virtual void BeginPlay() override;

	// 반사판의 회전 중심
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<USceneComponent> ReflectorPivot;

	// 실제 반사판 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> ReflectiveMesh;

	// 사이드뷰의 X-Z 평면에서 회전하도록 기본 축을 로컬 Y로 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser")
	FVector RotationAxis = FVector(0.0f, 1.0f, 0.0f);

	// 상호작용 한 번에 회전할 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser", meta = (ClampMin = "0.1", ClampMax = "360.0", Units = "deg"))
	float RotationStepAngle = 45.0f;

private:
	// 초기화할 때 돌아갈 피벗의 상대 회전
	FQuat InitialPivotRotation = FQuat::Identity;

	bool bHasInitialPivotRotation = false;
};
