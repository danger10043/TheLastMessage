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

	// +수정----
	// 이미 맞힌 스위치를 꺼도 진행은 유지하고 해당 레버만 복귀
	if (!bActive)
	{
		BeginFailureAnimation(Switch);
		return;
	}

	// 순서를 틀렸다면 잘못 누른 레버가 넘어간 뒤 원위치로 복귀
	// +--------
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
	// +추가----
	PlayGroupFailureSound();
	// +--------

	// 실패 연출 중 다른 레버를 누르지 못하도록 잠금
	SetGroupLeverInputLocked(true);

	// 잘못 누른 레버가 끝까지 넘어간 뒤 잠깐 유지
	const float Delay = FMath::Max(Switch->GetRemainingLeverAnimationTime() + FailureHoldTime, 0.01f);

	// +수정----
	// 이미 맞힌 구간의 레버는 켜진 상태로, 나머지 레버는 꺼진 상태로 되돌린다.
	const int32 SwitchIndex = GroupSwitches.IndexOfByKey(Switch);
	const bool bReturnActive = SwitchIndex != INDEX_NONE && SwitchIndex < NextSwitchIndex;
	GetWorldTimerManager().SetTimer(
		FailureReturnTimerHandle,
		FTimerDelegate::CreateUObject(this, &ASequentialSwitchGroup::PlayFailureReturn,
			TWeakObjectPtr<AToggleSwitch>(Switch), bReturnActive),
		Delay,
		false);
	// +--------
}

// +수정----
void ASequentialSwitchGroup::PlayFailureReturn(TWeakObjectPtr<AToggleSwitch> FailedSwitch, bool bReturnActive)
{
	if (!bIsFailureAnimating)
	{
		return;
	}

	// 연출 도중 외부에서 그룹이 완료됐다면 완료 상태와 입력 잠금 유지
	if (bIsCompleted)
	{
		FinishFailureAnimation();
		return;
	}

	float ReturnTime = 0.0f;

	// 진행 인덱스와 다른 레버를 건드리지 않고 오입력 레버 하나만 복귀시킨다.
	AToggleSwitch* Switch = FailedSwitch.Get();
	if (IsValid(Switch) && GroupSwitches.Contains(Switch))
	{
		Switch->ApplyActiveState(bReturnActive);
		ReturnTime = Switch->GetRemainingLeverAnimationTime();
	}

	UpdateGroupIndicator();

	// 오입력 레버가 돌아온 뒤 다음 정답 입력을 받는다.
	GetWorldTimerManager().SetTimer(
		FailureUnlockTimerHandle, 
		this, 
		&ASequentialSwitchGroup::FinishFailureAnimation, 
		FMath::Max(ReturnTime + 0.02f, 0.01f),
		false);
}
// +--------

void ASequentialSwitchGroup::FinishFailureAnimation()
{
	bIsFailureAnimating = false;

	// +수정----
	SetGroupLeverInputLocked(bIsCompleted);
	// +--------
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

void ASequentialSwitchGroup::RestoreProgress(const FProgressActorState& State)
{
    GetWorldTimerManager().ClearTimer(FailureReturnTimerHandle);
    GetWorldTimerManager().ClearTimer(FailureUnlockTimerHandle);
    bIsFailureAnimating = false;
    NextSwitchIndex = State.bCompleted ? GroupSwitches.Num() : 0;
    Super::RestoreProgress(State);
}
