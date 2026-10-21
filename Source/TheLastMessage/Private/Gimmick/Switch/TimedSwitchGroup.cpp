// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/TimedSwitchGroup.h"

#include "Engine/World.h"
#include "Gimmick/Switch/ToggleSwitch.h"

bool ATimedSwitchGroup::LoadTimedSwitchConfig()
{
	if (!TimedSwitchConfigRow.DataTable || TimedSwitchConfigRow.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: TimedSwitchConfigRow의 테이블과 행을 지정해주세요."), *GetName());

		return false;
	}

	const FTimedSwitchConfigRow* LoadedRow = TimedSwitchConfigRow.GetRow<FTimedSwitchConfigRow>(TEXT("ATimedSwitchGroup::LoadTimedSwitchConfig"));

	if (!LoadedRow)
	{
		return false;
	}

	if (!FMath::IsFinite(LoadedRow->TimeLimit) || LoadedRow->TimeLimit <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: TimeLimit은 0보다 큰 유효한 값이어야 합니다."), *GetName());

		return false;
	}

	if (!FMath::IsFinite(LoadedRow->FastBlinkThreshold) || LoadedRow->FastBlinkThreshold < 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: FastBlinkThreshold는 0 이상의 유효한 값이어야 합니다."), *GetName());

		return false;
	}

	TimedSwitchConfig = *LoadedRow;
	bConfigLoaded = true;

	return true;
}

void ATimedSwitchGroup::StartChallengeTimer()
{
	// 이미 진행 중이면 처음 시작한 시간을 유지
	if (!bIsEnabled || bIsCompleted || bIsResetting || bIsTimerRunning || !bConfigLoaded)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bIsTimerRunning = true;

	// 도전 제한 시간 시작
	World->GetTimerManager().SetTimer(
		ChallengeTimerHandle,
		this,
		&ATimedSwitchGroup::HandleTimeExpired,
		TimedSwitchConfig.TimeLimit,
		false);

	// 발광 상태를 주기적으로 갱신
	World->GetTimerManager().SetTimer(
		GlowUpdateTimerHandle,
		this,
		&ATimedSwitchGroup::UpdateSwitchGlow,
		0.05f,
		true);

	// 외부에서 타이머를 시작한 경우에도 진행 중 색상 반영
	UpdateGroupIndicator();

	// 첫 갱신까지 기다리지 않고 즉시 표시
	UpdateSwitchGlow();
}

void ATimedSwitchGroup::EvaluateTimedCompletion()
{
	if (!bIsEnabled || bIsCompleted || bIsResetting || !bIsTimerRunning)
	{
		return;
	}

	// 시간이 끝난 입력을 성공으로 처리하지 않음
	if (GetRemainingTime() <= 0.0f)
	{
		HandleTimeExpired();
		return;
	}

	if (!AreAllSwitchesActive())
	{
		return;
	}

	// 성공하면 시간 초과 처리가 발생하지 않도록 타이머 종료
	GetWorld()->GetTimerManager().ClearTimer(ChallengeTimerHandle);
	bIsTimerRunning = false;

	// 성공하면 타이머 발광 종료
	StopSwitchGlow();

	// 부모에서 완료 상태 기록과 연결된 문 활성화
	CompleteGroup();
}

float ATimedSwitchGroup::GetRemainingTime() const
{
	UWorld* World = GetWorld();

	if (!bIsTimerRunning || !World)
	{
		return 0.0f;
	}

	const float RemainingTime = World->GetTimerManager().GetTimerRemaining(ChallengeTimerHandle);

	return FMath::Max(0.0f, RemainingTime);
}

void ATimedSwitchGroup::ResetGroup()
{
	if (bIsCompleted || bIsResetting)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChallengeTimerHandle);
	}

	bIsTimerRunning = false;

	// 실패하거나 외부에서 초기화하면 타이머 발광 종료
	StopSwitchGlow();

	// 모든 스위치와 그룹 상태 초기화
	Super::ResetGroup();
}

void ATimedSwitchGroup::BeginPlay()
{
	Super::BeginPlay();

	if (!LoadTimedSwitchConfig())
	{
		return;
	}

	// 시작 시 이미 켜진 스위치가 있다면 도전 시작
	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (IsValid(Switch) && Switch->IsSwitchActive())
		{
			StartChallengeTimer();
			EvaluateTimedCompletion();
			break;
		}
	}
}

void ATimedSwitchGroup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 액터 종료 시 남아 있는 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChallengeTimerHandle);
	}

	bIsTimerRunning = false;
	StopSwitchGlow();

	Super::EndPlay(EndPlayReason);
}

void ATimedSwitchGroup::HandleSwitchStateChanged(AGimmickBase* Source, bool bActive)
{
	if (!bIsEnabled || bIsCompleted || bIsResetting || !bConfigLoaded)
	{
		return;
	}

	AToggleSwitch* ChangedSwitch = Cast<AToggleSwitch>(Source);

	if (!IsValid(ChangedSwitch) || !GroupSwitches.Contains(ChangedSwitch))
	{
		return;
	}

	// 처음 스위치를 켰을 때만 시작
	if (bActive && !bIsTimerRunning)
	{
		StartChallengeTimer();
	}

	// 꺼진 경우에도 타이머는 유지하고 현재 상태만 재확인
	EvaluateTimedCompletion();
}

bool ATimedSwitchGroup::IsGroupInProgress() const
{
	return bIsTimerRunning;
}

void ATimedSwitchGroup::HandleTimeExpired()
{
	if (bIsCompleted || bIsResetting || !bIsTimerRunning)
	{
		return;
	}

	ResetGroup();
}

void ATimedSwitchGroup::UpdateSwitchGlow()
{
	if (!bIsTimerRunning || bIsCompleted || bIsResetting)
	{
		StopSwitchGlow();
		return;
	}

	const float RemainingTime = GetRemainingTime();

	const bool bFastBlink = TimedSwitchConfig.FastBlinkThreshold > 0.0f && RemainingTime <= TimedSwitchConfig.FastBlinkThreshold;

	// 한 주기 중 절반은 켜지고, 나머지 절반은 꺼짐
	const float BlinkPeriod = bFastBlink ? 0.4f : 1.0f;

	const float ElapsedTime = FMath::Max(0.0f, TimedSwitchConfig.TimeLimit - RemainingTime);

	const float Phase = FMath::Fmod(ElapsedTime, BlinkPeriod);

	const bool bGlowOn = Phase < BlinkPeriod * 0.5f;

	const float GlowAlpha = bGlowOn ? 1.0f : 0.0f;

	// 그룹에 속한 모든 스위치를 같은 박자로 점멸
	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (IsValid(Switch))
		{
			Switch->SetTimerGlow(true, GlowAlpha);
		}
	}
}

void ATimedSwitchGroup::StopSwitchGlow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GlowUpdateTimerHandle);
	}

	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (IsValid(Switch))
		{
			Switch->SetTimerGlow(false, 0.0f);
		}
	}
}
