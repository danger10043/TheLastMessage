// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "PressurePlate.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTimelineComponent;
class UCurveFloat;
class UMaterialInstanceDynamic;
/**
 * 
 */
 // 허용된 물체가 올라오면 연결 장치를 활성화하는 발판
UCLASS(Blueprintable)
class THELASTMESSAGE_API APressurePlate : public AGimmickBase
{
	GENERATED_BODY()
	
public:
    virtual FProgressActorState CaptureProgress() const override;
    virtual void RefreshRestoredState() override;
    virtual void FinishRestoreProgress(const FProgressActorState& State) override;
    virtual int32 GetRestorePriority() const override { return 3; } APressurePlate();

    // 활성 상태에 따라 버튼의 이동 방향 변경
    virtual void ApplyActiveState(bool bNewActive) override;

	// 해당 액터가 발판을 누를 수 있는지 검사
	UFUNCTION(BlueprintPure, Category = "Gimmick|PressurePlate")
	bool CanPress(AActor* Actor) const;

    // 발판을 누르는 액터 등록
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PressurePlate")
    void HandlePlateEnter(AActor* Actor);

    // 발판에서 내려간 액터 제거
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PressurePlate")
    void HandlePlateExit(AActor* Actor);

    // 현재 감지 목록에 따라 활성 상태 갱신
    UFUNCTION(BlueprintCallable, Category = "Gimmick|PressurePlate")
    void UpdatePressedState();

    // 기능을 다시 켰을 때 이미 올라와 있는 물체도 확인
    virtual void SetGimmickEnabled(bool bNewEnabled) override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 발판 위의 물체를 감지하는 영역
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UBoxComponent> PlateTrigger;

    // 움직이지 않는 발판 베이스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> BaseMesh;

    // 눌렸을 때 아래로 움직이는 버튼
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> ButtonMesh;

    // 발판을 누를 수 있는 액터 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate")
    TArray<TSubclassOf<AActor>> AllowedActorClasses;

    // 현재 발판을 누르고 있는 액터
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AActor>> PressingActors;

    // 버튼 이동 재생을 담당
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UTimelineComponent> ButtonTimeline;

    // 원래 위치를 기준으로 눌렸을 때 이동할 거리
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate|Animation")
    FVector ButtonPressedOffset = FVector(0.0f, 0.0f, -5.0f);

    // 완전히 눌리거나 올라오는 데 걸리는 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate|Animation", meta = (ClampMin = "0.01", Units = "s"))
    float ButtonMoveDuration = 0.15f;

    // 표시등 재질이 버튼 메시에 있으면 체크
// 베이스 메시에 있으면 체크 해제
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate|Appearance")
    bool bIndicatorOnButtonMesh = false;

    // 선택한 메시에서 표시등 재질이 들어 있는 슬롯 번호
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate|Appearance", meta = (ClampMin = "0"))
    int32 IndicatorMaterialIndex = 0;

    // 버튼이 눌렸을 때 표시등의 발광 세기
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate|Appearance", meta = (ClampMin = "0.0"))
    float IndicatorOnStrength = 5.0f;

private:
    // 현재 감지 영역에 있는 액터를 기준으로 목록 재구성
    void RebuildPressingActors();

    // 감지 영역 진입 이벤트
    UFUNCTION()
    void HandleTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    // 감지 영역 이탈 이벤트
    UFUNCTION()
    void HandleTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

    // 발판 위의 액터가 파괴되거나 레벨에서 제거될 때 처리
    UFUNCTION()
    void HandlePressingActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

    // 실행 중 생성한 타임라인 커브 유지
    UPROPERTY(Transient)
    TObjectPtr<UCurveFloat> ButtonMovementCurve;

    // 게임 시작 시 버튼의 상대 위치
    FVector ButtonRestLocation = FVector::ZeroVector;

    // 타임라인 초기화 완료 여부
    bool bButtonAnimationReady = false;

    // 타임라인 진행도에 따라 버튼 위치 갱신
    UFUNCTION()
    void UpdateButtonLocation(float Alpha);

    // 저장 상태 복구 시 현재 활성 상태에 맞게 즉시 배치
    void SnapButtonToCurrentState();

    // 표시등을 개별 제어하기 위한 동적 머티리얼
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PlateIndicatorMaterial;

    // 현재 활성 상태에 따라 표시등 갱신
    void UpdatePlateIndicator();

};
