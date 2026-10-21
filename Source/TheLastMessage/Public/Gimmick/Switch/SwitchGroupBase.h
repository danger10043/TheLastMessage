// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "SwitchGroupBase.generated.h"

class AToggleSwitch;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
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

	// 그룹 종류에 따른 진행 중 상태 확인
	virtual bool IsGroupInProgress() const;

	// 현재 그룹 상태를 표시등 색상에 반영
	void UpdateGroupIndicator();

	// 그룹의 외형과 표시등
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> GroupMesh;

	// 표시등 재질이 들어 있는 슬롯 번호
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|SwitchGroup|Indicator", meta = (ClampMin = "0"))
	int32 IndicatorMaterialIndex = 1;

	// 맵에서 그룹에 속할 스위치를 지정
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SwitchGroup")
	TArray<TObjectPtr<AToggleSwitch>> GroupSwitches;

	// 성공 이후 완료 상태 유지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SwitchGroup")
	bool bIsCompleted = false;

	// 초기화 중 발생하는 이벤트의 재판정 방지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|SwitchGroup")
	bool bIsResetting = false;

private:
	// 자식의 판정이 끝난 다음 표시등 갱신
	UFUNCTION()
	void HandleGroupSwitchStateChanged(AGimmickBase* Source, bool bActive);

	// 시작할 때 찾아두고 색상 변경에 재사용
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroupIndicatorMaterial;

};
