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
