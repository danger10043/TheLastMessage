// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Plate/PressurePlate.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/PushPull/HeavyObject.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInstanceDynamic.h"

APressurePlate::APressurePlate()
{
    PrimaryActorTick.bCanEverTick = false;

    // 직접 조작하지 않고 물체 감지로 작동
    bSupportsInteraction = false;

    // 발판의 고정된 받침 부분
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(SceneRoot);
    BaseMesh->SetCollisionProfileName(TEXT("BlockAll"));
    BaseMesh->SetSimulatePhysics(false);

    // 버튼은 외형만 움직이고 물리 시뮬레이션은 사용하지 않음
    ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
    ButtonMesh->SetupAttachment(SceneRoot);
    ButtonMesh->SetMobility(EComponentMobility::Movable);
    ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ButtonMesh->SetSimulatePhysics(false);

    ButtonTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("ButtonTimeline"));

    ButtonTimeline->SetAutoActivate(false);

    PlateTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PlateTrigger"));

    PlateTrigger->SetupAttachment(SceneRoot);

    // 발판의 기준 위치에서 위쪽 공간을 감지
    PlateTrigger->SetBoxExtent(FVector(100.0f, 100.0f, 30.0f));
    PlateTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));

    // 물체를 막지 않고 겹침만 감지
    PlateTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PlateTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    PlateTrigger->SetCollisionResponseToAllChannels(ECR_Overlap);
    PlateTrigger->SetGenerateOverlapEvents(true);
    PlateTrigger->SetCanEverAffectNavigation(false);

    // 기본적으로 무게 상자와 그 자식 클래스를 허용
    AllowedActorClasses.Add(AHeavyObject::StaticClass());
}

void APressurePlate::ApplyActiveState(bool bNewActive)
{
    // +추가----
    const bool bPreviousActive = bIsActive;
    // +--------
    // 기존 활성 상태 변경과 연결 장치 알림 유지
    Super::ApplyActiveState(bNewActive);

    // 버튼의 활성 상태에 맞춰 표시등 갱신
    UpdatePlateIndicator();

    if (!bButtonAnimationReady || !IsValid(ButtonTimeline))
    {
        return;
    }

    // 현재 진행 위치에서 방향만 바꾸므로
    // 움직이는 도중 내려가거나 다시 올라와도 이어서 재생
    if (bIsActive)
    {
        ButtonTimeline->Play();
        // +추가----
        if (!bPreviousActive) PlayGimmickOneShot(PlatePressSound.Get(), ButtonMesh.Get());
        // +--------
    }
    else
    {
        ButtonTimeline->Reverse();
    }
}

bool APressurePlate::CanPress(AActor * Actor) const
{
    if (!bIsEnabled || !IsValid(Actor) || Actor == this || Actor->IsActorBeingDestroyed())
    {
        return false;
    }

    for (const TSubclassOf<AActor>& AllowedClass : AllowedActorClasses)
    {
        if (AllowedClass && Actor->IsA(AllowedClass.Get()))
        {
            return true;
        }
    }

    return false;
}

void APressurePlate::HandlePlateEnter(AActor* Actor)
{
    if (!CanPress(Actor) || !IsValid(PlateTrigger) || !PlateTrigger->IsOverlappingActor(Actor))
    {
        return;
    }

    // 같은 액터의 여러 컴포넌트가 겹쳐도 한 번만 등록
    PressingActors.AddUnique(TWeakObjectPtr<AActor>(Actor));

    Actor->OnEndPlay.AddUniqueDynamic(this, &APressurePlate::HandlePressingActorEndPlay);

    UpdatePressedState();
}

void APressurePlate::HandlePlateExit(AActor * Actor)
{
    // 다른 컴포넌트가 아직 겹쳐 있다면 등록 유지
    if (IsValid(Actor) && IsValid(PlateTrigger) && PlateTrigger->IsOverlappingActor(Actor))
    {
        return;
    }

    if (IsValid(Actor))
    {
        Actor->OnEndPlay.RemoveDynamic(this, &APressurePlate::HandlePressingActorEndPlay);
    }

    PressingActors.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Entry)
        {
            return !Entry.IsValid() || Entry.Get() == Actor;
        }
    );

    UpdatePressedState();
}

void APressurePlate::UpdatePressedState()
{
    // 제거되었거나 더 이상 발판에 올라와 있지 않은 액터 정리
    for (int32 Index = PressingActors.Num() - 1; Index >= 0; --Index)
    {
        AActor* Actor = PressingActors[Index].Get();

        const bool bStillPressing = CanPress(Actor) && IsValid(PlateTrigger) && PlateTrigger->IsOverlappingActor(Actor);

        if (!bStillPressing)
        {
            if (IsValid(Actor))
            {
                Actor->OnEndPlay.RemoveDynamic(this, &APressurePlate::HandlePressingActorEndPlay);
            }

            PressingActors.RemoveAt(Index);
        }
    }

    // 허용된 물체가 하나 이상 남아 있으면 활성화
    const bool bShouldBeActive = bIsEnabled && !PressingActors.IsEmpty();

    // 부모가 활성 상태와 연결 장치 알림을 처리
    ApplyActiveState(bShouldBeActive);
}

void APressurePlate::SetGimmickEnabled(bool bNewEnabled)
{
    Super::SetGimmickEnabled(bNewEnabled);

    if (HasActorBegunPlay())
    {
        // 다시 활성화할 때 현재 올라와 있는 물체를 기준으로 복구
        RebuildPressingActors();
    }
}

void APressurePlate::BeginPlay()
{
    Super::BeginPlay();

    // 표시등 재질이 들어 있는 메시 선택
    UStaticMeshComponent* IndicatorMesh = bIndicatorOnButtonMesh ? ButtonMesh.Get() : BaseMesh.Get();

    if (IsValid(IndicatorMesh))
    {
        // 부모에서 생성한 동적 머티리얼 재사용
        PlateIndicatorMaterial = Cast<UMaterialInstanceDynamic>(IndicatorMesh->GetMaterial(IndicatorMaterialIndex));

        if (!IsValid(PlateIndicatorMaterial))
        {
            PlateIndicatorMaterial = IndicatorMesh->CreateDynamicMaterialInstance(IndicatorMaterialIndex);
        }
    }

    if (!IsValid(PlateIndicatorMaterial))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 무게 발판의 표시등 메시와 재질 슬롯을 확인해주세요."), *GetName());
    }

    // 게임 시작 시 현재 상태에 맞게 표시등 초기화
    UpdatePlateIndicator();

    // BP에서 배치한 버튼의 시작 위치 저장
    ButtonRestLocation = ButtonMesh->GetRelativeLocation();

    // 0초에는 0, 1초에는 1인 진행도 커브 생성
    ButtonMovementCurve = NewObject<UCurveFloat>(this);

    ButtonMovementCurve->FloatCurve.AddKey(0.0f, 0.0f);
    ButtonMovementCurve->FloatCurve.AddKey(1.0f, 1.0f);

    FOnTimelineFloat UpdateDelegate;
    UpdateDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(APressurePlate, UpdateButtonLocation));

    ButtonTimeline->AddInterpFloat(ButtonMovementCurve, UpdateDelegate);

    ButtonTimeline->SetLooping(false);
    ButtonTimeline->SetTimelineLengthMode(TL_TimelineLength);
    ButtonTimeline->SetTimelineLength(1.0f);

    // 비정상적인 설정값이 들어오면 기본 시간 사용
    const float SafeDuration = FMath::IsFinite(ButtonMoveDuration) ? FMath::Max(ButtonMoveDuration, 0.01f) : 0.15f;

    ButtonTimeline->SetPlayRate(1.0f / SafeDuration);

    bButtonAnimationReady = true;

    PlateTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &APressurePlate::HandleTriggerBeginOverlap);

    PlateTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &APressurePlate::HandleTriggerEndOverlap);

    // 시작부터 올라와 있는 물체도 확인
    RebuildPressingActors();

    // 시작 상태에 맞는 위치로 배치
    SnapButtonToCurrentState();
}

void APressurePlate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(PlateTrigger))
    {
        PlateTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &APressurePlate::HandleTriggerBeginOverlap);

        PlateTrigger->OnComponentEndOverlap.RemoveDynamic(this, &APressurePlate::HandleTriggerEndOverlap);
    }

    for (const TWeakObjectPtr<AActor>& Entry : PressingActors)
    {
        if (AActor* Actor = Entry.Get())
        {
            Actor->OnEndPlay.RemoveDynamic(this, &APressurePlate::HandlePressingActorEndPlay);
        }
    }

    PressingActors.Reset();

    ApplyActiveState(false);

    if (IsValid(ButtonTimeline))
    {
        ButtonTimeline->Stop();
    }

    bButtonAnimationReady = false;

    Super::EndPlay(EndPlayReason);
}

void APressurePlate::RebuildPressingActors()
{
    // 기존 등록과 종료 이벤트 연결 정리
    for (const TWeakObjectPtr<AActor>& Entry : PressingActors)
    {
        if (AActor* Actor = Entry.Get())
        {
            Actor->OnEndPlay.RemoveDynamic(this, &APressurePlate::HandlePressingActorEndPlay);
        }
    }

    PressingActors.Reset();

    if (bIsEnabled && IsValid(PlateTrigger))
    {
        TArray<AActor*> OverlappingActors;
        PlateTrigger->GetOverlappingActors(OverlappingActors);

        for (AActor* Actor : OverlappingActors)
        {
            HandlePlateEnter(Actor);
        }
    }

    UpdatePressedState();
}

void APressurePlate::HandleTriggerBeginOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
    HandlePlateEnter(OtherActor);
}

void APressurePlate::HandleTriggerEndOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex)
{
    HandlePlateExit(OtherActor);
}

void APressurePlate::HandlePressingActorEndPlay(AActor * Actor, EEndPlayReason::Type EndPlayReason)
{
    // 파괴 중에는 약한 참조가 이미 무효화될 수 있으므로 함께 정리
    PressingActors.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Entry)
        {
            return !Entry.IsValid() || Entry.Get() == Actor;
        }
    );

    UpdatePressedState();
}

void APressurePlate::UpdateButtonLocation(float Alpha)
{
    if (!IsValid(ButtonMesh))
    {
        return;
    }

    const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    // 출발과 도착 부분을 부드럽게 처리
    const float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, ClampedAlpha);

    const FVector NewLocation = ButtonRestLocation + ButtonPressedOffset * SmoothedAlpha;

    ButtonMesh->SetRelativeLocation(NewLocation);
}

void APressurePlate::SnapButtonToCurrentState()
{
    if (!bButtonAnimationReady || !IsValid(ButtonTimeline))
    {
        return;
    }

    ButtonTimeline->Stop();

    const float Position = bIsActive ? 1.0f : 0.0f;

    // 타임라인 위치와 실제 버튼 위치를 함께 동기화
    ButtonTimeline->SetPlaybackPosition(Position, false, false);
    UpdateButtonLocation(Position);
}

void APressurePlate::UpdatePlateIndicator()
{
    if (!IsValid(PlateIndicatorMaterial))
    {
        return;
    }

    const bool bShouldLight = bIsEnabled && bIsActive;

    // 안 눌렸으면 소등, 눌렸으면 노란불 점등
    const FLinearColor LightColor = bShouldLight ? FLinearColor::Yellow : FLinearColor::Black;

    PlateIndicatorMaterial->SetVectorParameterValue(TEXT("IndicatorColor"), LightColor);

    // 발광 세기는 유지하고 색상으로 점등과 소등 처리
    PlateIndicatorMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), IndicatorOnStrength);
}

FProgressActorState APressurePlate::CaptureProgress() const
{
    FProgressActorState State = Super::CaptureProgress();
    State.bActive = false; // Contacts are rebuilt from the restored world, not latched from the save.
    return State;
}

void APressurePlate::RefreshRestoredState()
{
    RebuildPressingActors();
    SnapButtonToCurrentState();
}

void APressurePlate::FinishRestoreProgress(const FProgressActorState& State)
{
    RebuildPressingActors();
    SnapButtonToCurrentState();

    Super::FinishRestoreProgress(State);
}
