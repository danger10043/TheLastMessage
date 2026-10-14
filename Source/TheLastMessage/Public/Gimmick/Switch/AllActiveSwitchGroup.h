// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Switch/SwitchGroupBase.h"
#include "AllActiveSwitchGroup.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AAllActiveSwitchGroup : public ASwitchGroupBase
{
	GENERATED_BODY()
	
public:
	// 모든 스위치가 켜져 있으면 완료 처리
	UFUNCTION(BlueprintCallable, Category = "Gimmick|SwitchGroup")
	void EvaluateCompletion();

protected:
	virtual void BeginPlay() override;

	// 부모의 이벤트 수신 함수를 재정의
	virtual void HandleSwitchStateChanged(AGimmickBase* Source, bool bActive) override;
};
