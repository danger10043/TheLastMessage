// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/ToggleSwitch.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AToggleSwitch::AToggleSwitch()
{
	// 스위치는 플레이어의 직접 상호작용을 지원
	bSupportsInteraction = true;

	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));

	SwitchMesh->SetupAttachment(SceneRoot);
}

void AToggleSwitch::ToggleSwitch()
{
	// 사용 불가능한 스위치는 상태를 바꾸지 않음
	if (!bIsEnabled)
	{
		return;
	}

	// 부모의 활성 상태 변경 처리를 사용
	ApplyActiveState(!bIsActive);
}

void AToggleSwitch::ResetSwitch()
{
	// 초기화는 스위치 사용 가능 여부와 관계없이 수행
	ApplyActiveState(false);
}

void AToggleSwitch::PerformInteraction(ACharacter * Player)
{
	// 부모의 공통 상호작용 조건 확인
	if (!CanInteractInternal(Player))
	{
		return;
	}

	ToggleSwitch();
}

void AToggleSwitch::SetTimerGlow(bool bEnabled, float Alpha)
{
	bIsTimerGlowActive = bEnabled;
	TimerGlowAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	// 현재 상호작용 강조 상태를 유지하며 발광만 다시 계산
	UpdateInteractionHighlight(bIsHighlighted);
}

float AToggleSwitch::CalculateGlowIntensity() const
{
	if (!bIsEnabled || !bSupportsInteraction)
	{
		return 0.0f;
	}

	if (bIsTimerGlowActive)
	{
		return GlowIntensity * TimerGlowAlpha;
	}

	// 타이머 연출이 끝나면 기존 상호작용 강조로 복귀
	return Super::CalculateGlowIntensity();
}