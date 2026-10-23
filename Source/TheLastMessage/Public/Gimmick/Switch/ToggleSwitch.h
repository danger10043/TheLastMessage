// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/GimmickBase.h"
#include "Gimmick/Data/ToggleSwitchConfigRow.h"
#include "ToggleSwitch.generated.h"

class UStaticMeshComponent;
class UTimelineComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AToggleSwitch : public AGimmickBase
{
	GENERATED_BODY()
	
public:
	AToggleSwitch();

	// 현재 활성 상태를 반전
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Switch")
	void ToggleSwitch();

	// 스위치를 꺼진 상태로 초기화
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Switch")
	void ResetSwitch();

	// 데이터 테이블 설정을 읽고 타임라인 구성
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Switch")
	bool LoadToggleSwitchConfig();

	// 부모의 실제 상호작용 함수를 재정의
	virtual void PerformInteraction(ACharacter* Player) override;

	virtual void ApplyActiveState(bool bNewActive) override;

	// 그룹의 C++ 코드에서 현재 상태를 읽는 보조 함수
	bool IsSwitchActive() const { return bIsActive; }

	// 시간 제한 그룹에서 타이머 발광 설정
	void SetTimerGlow(bool bEnabled, float Alpha);

protected:
	virtual void BeginPlay() override;

	// 스위치 외형
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UStaticMeshComponent> SwitchMesh;

	// BP에 만들어둔 회전축 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LeverPivotComponent;

	// 레버 회전 재생을 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Components")
	TObjectPtr<UTimelineComponent> LeverTimeline;

	// 사용할 데이터 테이블과 행
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
	FDataTableRowHandle ToggleSwitchConfigRow;

	// 데이터 테이블에서 읽은 설정
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Switch")
	FToggleSwitchConfigRow ToggleSwitchConfig;

private:
	void UpdateSwitchIndicator();

	// 타임라인의 진행도로 레버 회전 적용
	UFUNCTION()
	void UpdateLeverRotation(float Alpha);

	// 타임라인 트랙 중복 등록 방지
	bool bTimelineConfigured = false;

	bool bIsTimerGlowActive = false;

	float TimerGlowAlpha = 0.0f;

};
