// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/AllActiveSwitchGroup.h"

#include "Gimmick/Switch/ToggleSwitch.h"

void AAllActiveSwitchGroup::EvaluateCompletion()
{
	if (!bIsEnabled || bIsCompleted || bIsResetting)
	{
		return;
	}

	if (AreAllSwitchesActive())
	{
		CompleteGroup();
	}
}

void AAllActiveSwitchGroup::BeginPlay()
{
	// 부모에서 스위치 이벤트를 연결
	Super::BeginPlay();

	// 시작할 때의 스위치 상태도 확인
	EvaluateCompletion();
}

void AAllActiveSwitchGroup::HandleSwitchStateChanged(
	AGimmickBase* Source,
	bool bActive)
{
	// 완료 이후 입력과 초기화 중 이벤트는 무시
	if (!bIsEnabled || bIsCompleted || bIsResetting)
	{
		return;
	}

	AToggleSwitch* ChangedSwitch = Cast<AToggleSwitch>(Source);

	if (!IsValid(ChangedSwitch))
	{
		return;
	}

	// 이 그룹에 등록된 스위치의 이벤트인지 확인
	if (!GroupSwitches.Contains(ChangedSwitch))
	{
		return;
	}

	EvaluateCompletion();
}
