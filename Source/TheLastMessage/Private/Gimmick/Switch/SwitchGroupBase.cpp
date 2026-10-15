// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/SwitchGroupBase.h"

#include "Gimmick/Switch/ToggleSwitch.h"

ASwitchGroupBase::ASwitchGroupBase()
{
	// 그룹 자체는 플레이어가 직접 누르는 대상이 아님
	bSupportsInteraction = false;
}

bool ASwitchGroupBase::AreAllSwitchesActive() const
{
	// 스위치가 없는 그룹은 성공으로 보지 않음
	if (GroupSwitches.IsEmpty())
	{
		return false;
	}

	for (AToggleSwitch* Switch : GroupSwitches)
	{
		// 비어 있는 항목이 있으면 성공으로 보지 않음
		if (!IsValid(Switch))
		{
			return false;
		}

		if (!Switch->IsSwitchActive())
		{
			return false;
		}
	}

	return true;
}

void ASwitchGroupBase::ResetGroup()
{
	// 성공한 그룹은 실패 초기화로 취소하지 않음
	if (bIsCompleted || bIsResetting)
	{
		return;
	}

	bIsResetting = true;

	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (!IsValid(Switch))
		{
			continue;
		}

		Switch->ResetSwitch();
	}

	ApplyActiveState(false);

	bIsResetting = false;
}

void ASwitchGroupBase::CompleteGroup()
{
	if (!bIsEnabled || bIsCompleted || bIsResetting)
	{
		return;
	}

	// 이벤트가 발생하기 전에 완료 상태부터 기록
	bIsCompleted = true;

	// 부모의 상태 변경·연결 장치 전달 처리 사용
	ApplyActiveState(true);
}

void ASwitchGroupBase::BeginPlay()
{
	Super::BeginPlay();

	BindSwitchEvents();
}

void ASwitchGroupBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 연결했던 스위치 이벤트 정리
	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (!IsValid(Switch))
		{
			continue;
		}

		Switch->OnActiveStateChanged.RemoveDynamic(this, &ASwitchGroupBase::HandleSwitchStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ASwitchGroupBase::BindSwitchEvents()
{
	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (!IsValid(Switch))
		{
			continue;
		}

		// 같은 연결이 중복 등록되지 않도록 처리
		Switch->OnActiveStateChanged.AddUniqueDynamic(this, &ASwitchGroupBase::HandleSwitchStateChanged);
	}
}

void ASwitchGroupBase::HandleSwitchStateChanged(AGimmickBase* Source, bool bActive)
{
	if (!bIsEnabled || bIsCompleted || bIsResetting)
	{
		return;
	}

	AToggleSwitch* ChangedSwitch = Cast<AToggleSwitch>(Source);

	if (!IsValid(ChangedSwitch))
	{
		return;
	}

	if (!GroupSwitches.Contains(ChangedSwitch))
	{
		return;
	}

	// 부모는 공통 확인까지만 수행.
	// 전체 활성·순차·시간 제한 판정은 자식에서 구현.
}
