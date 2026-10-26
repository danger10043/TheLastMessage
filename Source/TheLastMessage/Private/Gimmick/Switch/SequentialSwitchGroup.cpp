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
	if (!bIsEnabled || bIsCompleted || bIsResetting || bIsFailureAnimating)
	{
		return;
	}

	// 이 그룹에 등록된 유효한 스위치만 처리
	if (!IsValid(Switch) || !GroupSwitches.Contains(Switch))
	{
		return;
	}

	// 켜진 스위치를 다시 끄면 기존처럼 진행 초기화
	if (!bActive)
	{
		ResetSequenceProgress();
		return;
	}

	// 순서를 틀렸다면 레버가 넘어간 뒤 초기화
	if (!IsExpectedSwitch(Switch))
	{
		BeginFailureAnimation(Switch);
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

	// 외부에서 초기화를 요청하면 대기 중인 연출 정리
	GetWorldTimerManager().ClearTimer(FailureReturnTimerHandle);

	GetWorldTimerManager().ClearTimer(FailureUnlockTimerHandle);

	bIsFailureAnimating = false;

	// 다음 입력을 첫 번째 스위치부터 다시 받음
	NextSwitchIndex = 0;

	// 부모에서 등록된 스위치를 모두 끄고 그룹 상태 초기화
	Super::ResetGroup();
}

void ASequentialSwitchGroup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FailureReturnTimerHandle);

		GetWorldTimerManager().ClearTimer(FailureUnlockTimerHandle);
	}

	bIsFailureAnimating = false;

	SetGroupLeverInputLocked(false);

	Super::EndPlay(EndPlayReason);
}

void ASequentialSwitchGroup::HandleSwitchStateChanged(AGimmickBase* Source, bool bActive)
{
	AToggleSwitch* ChangedSwitch = Cast<AToggleSwitch>(Source);

	ProcessSwitchInput(ChangedSwitch, bActive);
}

void ASequentialSwitchGroup::BeginFailureAnimation(AToggleSwitch* Switch)
{
	if (!IsValid(Switch) || bIsFailureAnimating)
	{
		return;
	}

	bIsFailureAnimating = true;

	// 실패 연출 중 다른 레버를 누르지 못하도록 잠금
	SetGroupLeverInputLocked(true);

	// 잘못 누른 레버가 끝까지 넘어간 뒤 잠깐 유지
	const float Delay = FMath::Max(Switch->GetRemainingLeverAnimationTime() + FailureHoldTime, 0.01f);

	GetWorldTimerManager().SetTimer(FailureReturnTimerHandle, this, &ASequentialSwitchGroup::PlayFailureReturn, Delay, false);
}

void ASequentialSwitchGroup::PlayFailureReturn()
{
	if (!bIsFailureAnimating)
	{
		return;
	}

	NextSwitchIndex = 0;

	// 입력 잠금을 유지하면서 기존 초기화로 복귀 재생
	Super::ResetGroup();

	float ReturnTime = 0.0f;

	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (!IsValid(Switch))
		{
			continue;
		}

		ReturnTime = FMath::Max(ReturnTime, Switch->GetRemainingLeverAnimationTime());
	}

	// 모든 레버가 돌아올 시간을 확보한 뒤 입력 재개
	GetWorldTimerManager().SetTimer(
		FailureUnlockTimerHandle, 
		this, 
		&ASequentialSwitchGroup::FinishFailureAnimation, 
		FMath::Max(ReturnTime + 0.02f, 0.01f),
		false);
}

void ASequentialSwitchGroup::FinishFailureAnimation()
{
	bIsFailureAnimating = false;

	SetGroupLeverInputLocked(false);
}

void ASequentialSwitchGroup::SetGroupLeverInputLocked(bool bLocked)
{
	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (IsValid(Switch))
		{
			Switch->SetLeverInputLocked(bLocked);
		}
	}
}
