// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/SwitchGroupBase.h"

#include "Gimmick/Switch/ToggleSwitch.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ASwitchGroupBase::ASwitchGroupBase()
{
	// 그룹 자체는 플레이어가 직접 누르는 대상이 아님
	bSupportsInteraction = false;

	GroupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroupMesh"));

	GroupMesh->SetupAttachment(SceneRoot);

	// 진행 상태를 표시하는 외형이므로 충돌은 사용하지 않음
	GroupMesh->SetCollisionProfileName(TEXT("NoCollision"));
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

	UpdateGroupIndicator();

}

void ASwitchGroupBase::CompleteGroup()
{
	if (!bIsEnabled || bIsCompleted || bIsResetting)
	{
		return;
	}

	// 이벤트가 발생하기 전에 완료 상태부터 기록
	bIsCompleted = true;

	UpdateGroupIndicator();

	// 부모의 상태 변경·연결 장치 전달 처리 사용
	ApplyActiveState(true);
}

void ASwitchGroupBase::BeginPlay()
{
	Super::BeginPlay();

	GroupIndicatorMaterial = Cast<UMaterialInstanceDynamic>(
		GroupMesh->GetMaterial(IndicatorMaterialIndex));

	if (!IsValid(GroupIndicatorMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: GroupMesh와 표시등 재질 슬롯을 확인해주세요."), *GetName());
	}

	BindSwitchEvents();

	UpdateGroupIndicator();

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

		Switch->OnActiveStateChanged.RemoveDynamic(this, &ASwitchGroupBase::HandleGroupSwitchStateChanged);
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

		// 공통 처리 함수에서 자식 판정과 표시등 갱신을 순서대로 실행
		Switch->OnActiveStateChanged.AddUniqueDynamic(this, &ASwitchGroupBase::HandleGroupSwitchStateChanged);
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

bool ASwitchGroupBase::IsGroupInProgress() const
{
	// 기본 규칙은 스위치가 하나라도 켜져 있으면 진행 중
	for (AToggleSwitch* Switch : GroupSwitches)
	{
		if (IsValid(Switch) && Switch->IsSwitchActive())
		{
			return true;
		}
	}

	return false;

}

void ASwitchGroupBase::UpdateGroupIndicator()
{
	if (!IsValid(GroupIndicatorMaterial))
	{
		return;
	}

	FLinearColor IndicatorColor = FLinearColor::Red;

	// 완료 상태를 가장 우선해서 표시
	if (bIsCompleted)
	{
		IndicatorColor = FLinearColor::Green;
	}
	else if (IsGroupInProgress())
	{
		IndicatorColor = FLinearColor::Yellow;
	}

	GroupIndicatorMaterial->SetVectorParameterValue(TEXT("IndicatorColor"), IndicatorColor);

}

void ASwitchGroupBase::HandleGroupSwitchStateChanged(AGimmickBase* Source, bool bActive)
{
	// 초기화가 끝난 뒤 한 번에 표시 갱신
	if (bIsResetting)
	{
		return;
	}

	// 전체 활성, 순차, 시간 제한 그룹이 각각 판정
	HandleSwitchStateChanged(Source, bActive);

	// 판정 결과에 맞는 색상 표시
	UpdateGroupIndicator();

}
