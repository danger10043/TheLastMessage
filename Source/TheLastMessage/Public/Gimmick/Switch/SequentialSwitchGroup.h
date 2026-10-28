// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Switch/SwitchGroupBase.h"
#include "TimerManager.h"
#include "SequentialSwitchGroup.generated.h"

class AToggleSwitch;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ASequentialSwitchGroup : public ASwitchGroupBase
{
	GENERATED_BODY()
	
public:
    virtual bool ResetIncompleteOnLoad() const override { return true; }
    virtual void RestoreProgress(const FProgressActorState& State) override;
	// 입력된 스위치가 다음 순서에 해당하는지 확인
	UFUNCTION(BlueprintPure, Category = "Gimmick|SequentialSwitchGroup")
	bool IsExpectedSwitch(AToggleSwitch* Switch) const;

	// 스위치 상태 변경에 따라 순서 판정
	UFUNCTION(BlueprintCallable, Category = "Gimmick|SequentialSwitchGroup")
	void ProcessSwitchInput(AToggleSwitch* Switch, bool bActive);

	// 순서 진행 상황과 스위치 상태 초기화
	UFUNCTION(BlueprintCallable, Category = "Gimmick|SequentialSwitchGroup")
	void ResetSequenceProgress();

	// 부모의 초기화 기능에 순서 인덱스 초기화 추가
	virtual void ResetGroup() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 부모에서 구독한 스위치 상태 변경 이벤트 수신
	virtual void HandleSwitchStateChanged(AGimmickBase* Source, bool bActive) override;

	// 다음에 켜야 할 스위치의 배열 인덱스
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SequentialSwitchGroup")
	int32 NextSwitchIndex = 0;

	// 잘못된 레버가 끝까지 넘어간 뒤 유지할 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|SequentialSwitchGroup", meta = (ClampMin = "0.0", Units = "s"))
	float FailureHoldTime = 0.1f;

private:
	// 오입력 레버의 움직임이 끝나기를 기다림
	void BeginFailureAnimation(AToggleSwitch* Switch);

	// 모든 레버를 원위치로 복귀
	void PlayFailureReturn();

	// 복귀가 끝나면 입력 잠금 해제
	void FinishFailureAnimation();

	void SetGroupLeverInputLocked(bool bLocked);

	bool bIsFailureAnimating = false;

	FTimerHandle FailureReturnTimerHandle;
	FTimerHandle FailureUnlockTimerHandle;

};
