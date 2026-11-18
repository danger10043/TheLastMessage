// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/DoorConfigRow.h"
#include "ElectricDoor.generated.h"

class UStaticMeshComponent;
class UTimelineComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AElectricDoor : public AGimmickBase
{
	GENERATED_BODY()

public:
    virtual void RefreshRestoredState() override;
    virtual void RestoreProgress(const FProgressActorState& State) override;
    AElectricDoor();

    UFUNCTION(BlueprintCallable, Category = "Gimmick|Door")
    bool LoadDoorConfig();

    UFUNCTION(BlueprintCallable, Category = "Gimmick|Door")
    void OpenDoor();

    UFUNCTION(BlueprintCallable, Category = "Gimmick|Door")
    void CloseDoor();

    // 부모의 활성 상태 변경을 문 동작에 연결
    virtual void ApplyActiveState(bool bNewActive) override;

protected:
    virtual void BeginPlay() override;
    // +추가----
    virtual void RefreshGimmickAudioState() override;
    UFUNCTION()
    void HandleDoorMotionFinished();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Audio")
    TObjectPtr<USoundBase> DoorOpenSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Audio")
    TObjectPtr<USoundBase> DoorCloseSound;
    // +--------

    // Timeline이 전달하는 진행률로 문 위치 갱신
    UFUNCTION()
    void UpdateDoorTransform(float Alpha);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
    TObjectPtr<UTimelineComponent> DoorTimeline;

    // SceneRoot를 기준으로 한 문 메시의 상대 Transform
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Door", meta = (MakeEditWidget = true))
    FTransform ClosedTransform = FTransform::Identity;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Door", meta = (MakeEditWidget = true))
    FTransform OpenTransform = FTransform::Identity;

    /** 한 번 개방 신호를 받으면 닫기 신호를 무시하고 열린 상태를 유지한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Door",
        meta = (DisplayName = "개방 후 열린 상태 유지"))
    bool bKeepOpenAfterActivation = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
    FDataTableRowHandle DoorConfigRow;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Door")
    FDoorConfigRow DoorConfig;

private:
    bool bTimelineConfigured = false;
    bool bOpenLatched = false;

};
