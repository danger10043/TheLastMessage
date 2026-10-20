// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Door/ElectricDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"

AElectricDoor::AElectricDoor()
{
    // 문은 플레이어가 직접 조작하지 않고 활성 신호를 받음
    bSupportsInteraction = false;

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));

    DoorMesh->SetupAttachment(SceneRoot);
    DoorMesh->SetMobility(EComponentMobility::Movable);
    DoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));

    // 기본값: 닫힌 위치에서 위로 300cm 이동
    OpenTransform.SetTranslation(FVector(0.0f, 0.0f, 300.0f));
}

bool AElectricDoor::LoadDoorConfig()
{
    if (!DoorConfigRow.DataTable || DoorConfigRow.RowName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: DoorConfigRow의 테이블과 행을 지정해주세요."), *GetName());

        return false;
    }

    const FDoorConfigRow* LoadedRow = DoorConfigRow.GetRow<FDoorConfigRow>(TEXT("AElectricDoor::LoadDoorConfig"));

    if (!LoadedRow)
    {
        return false;
    }

    if (!FMath::IsFinite(LoadedRow->MovementDuration)
        || LoadedRow->MovementDuration <= 0.0f
        || !IsValid(LoadedRow->MovementCurve.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 이동 시간은 0보다 커야 하고 이동 곡선이 필요합니다."), *GetName());

        return false;
    }

    UCurveFloat* Curve = LoadedRow->MovementCurve.Get();

    float MinTime = 0.0f;
    float MaxTime = 0.0f;

    Curve->GetTimeRange(MinTime, MaxTime);

    // 타임라인은 0~1 구간을 이동 시간에 맞춰 재생
    if (!FMath::IsNearlyEqual(MinTime, 0.0f)
        || !FMath::IsNearlyEqual(MaxTime, 1.0f)
        || !FMath::IsNearlyEqual(Curve->GetFloatValue(0.0f), 0.0f)
        || !FMath::IsNearlyEqual(Curve->GetFloatValue(1.0f), 1.0f))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 이동 곡선의 시작은 Time 0 / Value 0, 끝은 Time 1 / Value 1이어야 합니다."), *GetName());

        return false;
    }

    DoorConfig = *LoadedRow;

    const FName TrackName(TEXT("DoorMovement"));

    if (!bTimelineConfigured)
    {
        FOnTimelineFloat UpdateDelegate;

        UpdateDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AElectricDoor, UpdateDoorTransform));

        DoorTimeline->AddInterpFloat(Curve, UpdateDelegate, NAME_None, TrackName);

        bTimelineConfigured = true;
    }
    else
    {
        // 설정을 다시 읽어도 트랙을 중복 추가하지 않음
        DoorTimeline->SetFloatCurve(Curve, TrackName);
    }

    DoorTimeline->SetLooping(false);
    DoorTimeline->SetTimelineLengthMode(TL_TimelineLength);
    DoorTimeline->SetTimelineLength(1.0f);

    DoorTimeline->SetPlayRate(1.0f / DoorConfig.MovementDuration);

    return true;
}

void AElectricDoor::OpenDoor()
{
    ApplyActiveState(true);
}

void AElectricDoor::CloseDoor()
{
    ApplyActiveState(false);
}

void AElectricDoor::ApplyActiveState(bool bNewActive)
{
    // 사용 불가 상태에서는 문을 여는 요청 차단
    // 문을 닫는 요청은 허용
    if (!bIsEnabled && bNewActive)
    {
        return;
    }

    // 같은 상태의 반복 처리 방지
    if (bIsActive == bNewActive)
    {
        return;
    }

    // 활성 상태 기록 및 연결 장치 알림
    Super::ApplyActiveState(bNewActive);

    // 초기화 전 신호는 상태만 기록
    if (!bTimelineConfigured)
    {
        return;
    }

    if (bIsActive)
    {
        DoorTimeline->Play();
    }
    else
    {
        DoorTimeline->Reverse();
    }
}

void AElectricDoor::BeginPlay()
{
    Super::BeginPlay();

    // 먼저 닫힌 Transform 적용
    UpdateDoorTransform(0.0f);

    if (!LoadDoorConfig())
    {
        return;
    }

    // BeginPlay 이전에 활성 신호를 받은 경우도 반영
    const float InitialPosition = bIsActive ? 1.0f : 0.0f;

    DoorTimeline->SetPlaybackPosition(InitialPosition, false, true);
}

void AElectricDoor::UpdateDoorTransform(float Alpha)
{
    const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    FTransform NewTransform;

    NewTransform.Blend(ClosedTransform, OpenTransform, ClampedAlpha);

    DoorMesh->SetRelativeTransform(NewTransform);
}
