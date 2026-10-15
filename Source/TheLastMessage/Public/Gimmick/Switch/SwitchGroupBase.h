// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "SwitchGroupBase.generated.h"

class AToggleSwitch;

/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ASwitchGroupBase : public AGimmickBase
{
	GENERATED_BODY()
	
public:
	ASwitchGroupBase();

	// 등록된 모든 스위치가 커져 있는지 확인
	UFUNCTION(BlueprintPure, Category = "Gimmick|SwitchGroup")
	bool AreAllSwitchesActive() const;

	// 실패 시 스위치와 그룹 상태 초기화
	UFUNCTION(BlueprintCallable, Category = "Gimmick|SwitchGroup")
	virtual void ResetGroup();

	// 자식의 성공 판정 후 호출
	UFUNCTION(BlueprintCallable, Category = "Gimmick|SwitchGroup")
	void CompleteGroup();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 스위치 상태 변경 이벤트 구독
	void BindSwitchEvents();

	// 이벤트를 받을 함수이므로 UFUNCTION이 필요
	// 자식 그룹에서 재정의하여 자신의 규칙 판정
	UFUNCTION()
	virtual void HandleSwitchStateChanged(AGimmickBase* Source, bool bActive);

	// 맵에서 그룹에 속할 스위치를 지정
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SwitchGroup")
	TArray<TObjectPtr<AToggleSwitch>> GroupSwitches;

	// 성공 이후 완료 상태 유지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SwitchGroup")
	bool bIsCompleted = false;

	// 초기화 중 발생하는 이벤트의 재판정 방지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SwitchGroup")
	bool bIsResetting = false;
};
