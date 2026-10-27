// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Plate/PressurePlate.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Gimmick/PushPull/HeavyObject.h"

APressurePlate::APressurePlate()
{
    PrimaryActorTick.bCanEverTick = false;

    // 직접 조작하지 않고 물체 감지로 작동
    bSupportsInteraction = false;

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

    PlateTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &APressurePlate::HandleTriggerBeginOverlap);

    PlateTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &APressurePlate::HandleTriggerEndOverlap);

    // 게임 시작부터 올라와 있는 물체도 확인
    RebuildPressingActors();
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
