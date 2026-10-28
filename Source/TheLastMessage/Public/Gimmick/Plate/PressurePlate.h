// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "PressurePlate.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
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
    virtual int32 GetRestorePriority() const override { return 3; }
	APressurePlate();

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

    // 발판을 누를 수 있는 액터 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|PressurePlate")
    TArray<TSubclassOf<AActor>> AllowedActorClasses;

    // 현재 발판을 누르고 있는 액터
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AActor>> PressingActors;

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

};
