// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Switch/ToggleSwitch.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"

AToggleSwitch::AToggleSwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	// 스위치는 플레이어의 직접 상호작용을 지원
	bSupportsInteraction = true;

	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));

	SwitchMesh->SetupAttachment(SceneRoot);

	LeverTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("LeverTimeline"));

	LeverTimeline->SetAutoActivate(false);
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

bool AToggleSwitch::LoadToggleSwitchConfig()
{
	if (!ToggleSwitchConfigRow.DataTable || ToggleSwitchConfigRow.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ToggleSwitchConfigRow의 테이블과 행을 지정해주세요."), *GetName());

		return false;
	}

	const FToggleSwitchConfigRow* LoadedRow = ToggleSwitchConfigRow.GetRow<FToggleSwitchConfigRow>(TEXT("AToggleSwitch::LoadToggleSwitchConfig"));

	if (!LoadedRow || !IsValid(LeverTimeline))
	{
		return false;
	}

	if (!FMath::IsFinite(LoadedRow->RotationDuration)
		|| LoadedRow->RotationDuration < 0.01f
		|| !FMath::IsFinite(LoadedRow->LeverOffPitch)
		|| !FMath::IsFinite(LoadedRow->LeverOnPitch)
		|| !IsValid(LoadedRow->LeverRotationCurve.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 회전 시간은 0.01초 이상이어야 하며, 유효한 각도와 회전 커브가 필요합니다."), *GetName());

		return false;
	}

	UCurveFloat* Curve = LoadedRow->LeverRotationCurve.Get();

	float MinTime = 0.0f;
	float MaxTime = 0.0f;

	Curve->GetTimeRange(MinTime, MaxTime);

	// 커브의 시간과 진행도를 0~1로 통일
	if (!FMath::IsNearlyEqual(MinTime, 0.0f)
		|| !FMath::IsNearlyEqual(MaxTime, 1.0f)
		|| !FMath::IsNearlyEqual(Curve->GetFloatValue(0.0f), 0.0f)
		|| !FMath::IsNearlyEqual(Curve->GetFloatValue(1.0f), 1.0f))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 회전 커브는 시작 Time 0 / Value 0, 끝 Time 1 / Value 1로 설정해주세요."), *GetName());

		return false;
	}

	ToggleSwitchConfig = *LoadedRow;

	const FName TrackName(TEXT("SwitchLeverRotation"));

	if (!bTimelineConfigured)
	{
		FOnTimelineFloat UpdateDelegate;

		UpdateDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AToggleSwitch, UpdateLeverRotation));

		LeverTimeline->AddInterpFloat(Curve, UpdateDelegate, NAME_None, TrackName);

		bTimelineConfigured = true;
	}
	else
	{
		// 설정을 다시 읽어도 트랙은 중복 추가하지 않음
		LeverTimeline->SetFloatCurve(Curve, TrackName);
	}

	LeverTimeline->SetLooping(false);
	LeverTimeline->SetTimelineLengthMode(TL_TimelineLength);
	LeverTimeline->SetTimelineLength(1.0f);

	// 커브의 1초 구간을 설정한 회전 시간에 맞춰 재생
	LeverTimeline->SetPlayRate(1.0f / ToggleSwitchConfig.RotationDuration);

	return true;
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

void AToggleSwitch::ApplyActiveState(bool bNewActive)
{
	Super::ApplyActiveState(bNewActive);

	UpdateSwitchIndicator();

	// 초기화 전에는 상태와 표시등만 갱신
	if (!bTimelineConfigured || !IsValid(LeverTimeline))
	{
		return;
	}

	// 그룹 판정 중 초기화될 수도 있으므로 실제 현재 상태 사용
	if (bIsActive)
	{
		LeverTimeline->Play();
	}
	else
	{
		LeverTimeline->Reverse();
	}
}

void AToggleSwitch::SetTimerGlow(bool bEnabled, float Alpha)
{
	const float NewAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	// 같은 점멸 상태가 반복 전달되면 재질을 다시 갱신하지 않음
	if (bIsTimerGlowActive == bEnabled && FMath::IsNearlyEqual(TimerGlowAlpha, NewAlpha))
	{
		return;
	}

	bIsTimerGlowActive = bEnabled;
	TimerGlowAlpha = NewAlpha;

	// 타이머 점멸은 표시등에만 적용
	UpdateSwitchIndicator();
}

void AToggleSwitch::BeginPlay()
{
	Super::BeginPlay();

	TInlineComponentArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);

	for (USceneComponent* Component : SceneComponents)
	{
		if (IsValid(Component) && Component->GetFName() == FName(TEXT("LeverPivot")))
		{
			LeverPivotComponent = Component;
			LeverPivotComponent->SetMobility(EComponentMobility::Movable);
			break;
		}
	}

	UpdateSwitchIndicator();

	if (!IsValid(LeverPivotComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: LeverPivot 컴포넌트를 찾지 못했습니다."), *GetName());

		return;
	}

	if (!LoadToggleSwitchConfig())
	{
		return;
	}

	// 시작 시 현재 활성 상태에 맞는 각도로 배치
	const float InitialPosition = bIsActive ? 1.0f : 0.0f;

	LeverTimeline->SetPlaybackPosition(InitialPosition, false, true);
}

void AToggleSwitch::UpdateSwitchIndicator()
{
	// 스위치의 실제 상태에 따라 기본 색상 결정
	const FLinearColor StateColor = bIsActive ? FLinearColor(0.0f, 1.0f, 0.0f) : FLinearColor(1.0f, 0.0f, 0.0f);

	// 타이머가 끝나면 항상 원래 밝기로 복귀
	const float Brightness = bIsTimerGlowActive ? TimerGlowAlpha : 1.0f;

	// 색상과 발광을 어둡게 하되 알파 값은 유지
	const FLinearColor TargetColor(StateColor.R * Brightness, StateColor.G * Brightness, StateColor.B * Brightness, StateColor.A);

	TInlineComponentArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);

	int32 UpdatedCount = 0;

	for (UMeshComponent* Mesh : MeshComponents)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}

		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Material = Mesh->GetMaterial(Slot);

			if (!IsValid(Material))
			{
				continue;
			}

			TArray<FMaterialParameterInfo> ParameterInfos;
			TArray<FGuid> ParameterIds;

			Material->GetAllVectorParameterInfo(ParameterInfos, ParameterIds);

			for (const FMaterialParameterInfo& Info : ParameterInfos)
			{
				if (Info.Name != FName(TEXT("IndicatorColor")))
				{
					continue;
				}

				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);

				if (!MID)
				{
					MID = Mesh->CreateDynamicMaterialInstance(Slot);
				}

				if (!IsValid(MID))
				{
					continue;
				}

				MID->SetVectorParameterValueByInfo(Info, TargetColor);

				++UpdatedCount;
				break;
			}
		}
	}

	if (UpdatedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: IndicatorColor를 가진 표시등 재질을 찾지 못했습니다."), *GetName());
	}
}

void AToggleSwitch::UpdateLeverRotation(float Alpha)
{
	if (!IsValid(LeverPivotComponent))
	{
		return;
	}

	const float Progress = FMath::Clamp(Alpha, 0.0f, 1.0f);

	FRotator Rotation = LeverPivotComponent->GetRelativeRotation();

	Rotation.Pitch = FMath::Lerp( ToggleSwitchConfig.LeverOffPitch, ToggleSwitchConfig.LeverOnPitch, Progress);

	LeverPivotComponent->SetRelativeRotation(Rotation);
}
