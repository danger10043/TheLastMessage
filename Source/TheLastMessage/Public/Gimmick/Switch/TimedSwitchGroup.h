// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gimmick/Switch/SwitchGroupBase.h"
#include "Gimmick/Data/TimedSwitchConfigRow.h"
#include "TimerManager.h"
#include "TimedSwitchGroup.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ATimedSwitchGroup : public ASwitchGroupBase
{
	GENERATED_BODY()
	
public:
    virtual bool ResetIncompleteOnLoad() const override { return true; }
    virtual void RestoreProgress(const FProgressActorState& State) override;
	// 데이터 테이블에서 제한 시간 설정 읽기
	UFUNCTION(BlueprintCallable, Category = "Gimmick|TimedSwitchGroup")
	bool LoadTimedSwitchConfig();

	// 첫 입력에서 제한 시간 시작
	UFUNCTION(BlueprintCallable, Category = "Gimmick|TimedSwitchGroup")
	void StartChallengeTimer();

	// 제한 시간 안에 모든 스위치가 켜졌는지 확인
	UFUNCTION(BlueprintCallable, Category = "Gimmick|TimedSwitchGroup")
	void EvaluateTimedCompletion();

	// 남은 제한 시간 반환
	UFUNCTION(BlueprintPure, Category = "Gimmick|TimedSwitchGroup")
	float GetRemainingTime() const;

	// 타이머와 부모의 그룹 상태를 함께 초기화
	virtual void ResetGroup() override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void HandleSwitchStateChanged(AGimmickBase* Source, bool bActive) override;

	// 타이머가 흐르는 동안 진행 중으로 표시
	virtual bool IsGroupInProgress() const override;

	// 제한 시간이 끝나면 호출
	UFUNCTION()
	void HandleTimeExpired();

protected:
	// 사용할 데이터 테이블과 행 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Config")
	FDataTableRowHandle TimedSwitchConfigRow;

	// 읽어온 제한 시간 설정
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|TimedSwitchGroup")
	FTimedSwitchConfigRow TimedSwitchConfig;

	// 현재 도전 타이머가 진행 중인지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Gimmick|TimedSwitchGroup")
	bool bIsTimerRunning = false;

	// 도전 제한 시간 관리
	FTimerHandle ChallengeTimerHandle;

	// 진행 중 발광 상태 갱신
	FTimerHandle GlowUpdateTimerHandle;

private:
	// 유효한 설정을 읽은 뒤에만 도전 시작
	bool bConfigLoaded = false;

	// 남은 시간으로 모든 스위치의 발광 상태 계산
	void UpdateSwitchGlow();

	// 타이머 발광을 종료하고 상호작용 강조로 복귀
	void StopSwitchGlow();

};
