// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Switch/SwitchGroupBase.h"
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
	// 부모에서 구독한 스위치 상태 변경 이벤트 수신
	virtual void HandleSwitchStateChanged(AGimmickBase* Source, bool bActive) override;

	// 다음에 켜야 할 스위치의 배열 인덱스
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SequentialSwitchGroup")
	int32 NextSwitchIndex = 0;

};
