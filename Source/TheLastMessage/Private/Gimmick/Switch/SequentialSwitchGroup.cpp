// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/SequentialSwitchGroup.h"

#include "Gimmick/Switch/ToggleSwitch.h"

bool ASequentialSwitchGroup::IsExpectedSwitch(AToggleSwitch* Switch) const
{
	if (!IsValid(Switch))
	{
		return false;
	}

	if (!GroupSwitches.IsValidIndex(NextSwitchIndex))
	{
		return false;
	}

	return GroupSwitches[NextSwitchIndex].Get() == Switch;
}

void ASequentialSwitchGroup::ProcessSwitchInput(AToggleSwitch* Switch, bool bActive)
{
	// 사용 불가, 완료 이후, 초기화 중에는 판정하지 않음
	if (!bIsEnabled || bIsCompleted || bIsResetting)
	{
		return;
	}

	// 이 그룹에 등록된 유효한 스위치만 처리
	if (!IsValid(Switch) || !GroupSwitches.Contains(Switch))
	{
		return;
	}

	// 완료 전에 스위치를 끄거나 순서를 틀리면 초기화
	if (!bActive || !IsExpectedSwitch(Switch))
	{
		ResetSequenceProgress();
		return;
	}

	// 올바른 스위치를 켰으므로 다음 순서로 진행
	++NextSwitchIndex;

	// 마지막 스위치까지 올바르게 켰으면 완료
	if (NextSwitchIndex == GroupSwitches.Num())
	{
		CompleteGroup();
	}
}

void ASequentialSwitchGroup::ResetSequenceProgress()
{
	ResetGroup();
}

void ASequentialSwitchGroup::ResetGroup()
{
	// 완료 상태는 유지하고, 초기화 중 중복 호출은 무시
	if (bIsCompleted || bIsResetting)
	{
		return;
	}

	// 다음 입력을 첫 번째 스위치부터 다시 받음
	NextSwitchIndex = 0;

	// 부모에서 등록된 스위치를 모두 끄고 그룹 상태 초기화
	Super::ResetGroup();
}

void ASequentialSwitchGroup::HandleSwitchStateChanged(AGimmickBase* Source, bool bActive)
{
	AToggleSwitch* ChangedSwitch = Cast<AToggleSwitch>(Source);

	ProcessSwitchInput(ChangedSwitch, bActive);
}