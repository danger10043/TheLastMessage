// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Laser/LaserReceiver.h"


#include "Components/StaticMeshComponent.h"
#include "Gimmick/Laser/LaserEmitter.h"

ALaserReceiver::ALaserReceiver()
{
	// 접촉 중인 발신기의 소멸 여부를 확인할 때만 Tick 사용
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 수신기는 레이저로 작동하는 장치
	bSupportsInteraction = false;

	ReceiverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReceiverMesh"));

	ReceiverMesh->SetupAttachment(SceneRoot);
	ReceiverMesh->SetMobility(EComponentMobility::Movable);

	// 반사판과 같은 기본 높이에 수신부 배치
	ReceiverMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	// 레이저 트레이스가 수신기 표면에 충돌하도록 설정
	ReceiverMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	ReceiverMesh->SetGenerateOverlapEvents(false);
}

void ALaserReceiver::BeginPlay()
{
	Super::BeginPlay();

	UpdateReceiverState();
}

void ALaserReceiver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateReceiverState();
}

void ALaserReceiver::SetLaserContact(ALaserEmitter* Emitter, bool bHasContact)
{
	if (!IsValid(Emitter))
	{
		// 이미 사라진 발신기가 있다면 목록 정리
		UpdateReceiverState();
		return;
	}

	const TWeakObjectPtr<ALaserEmitter> WeakEmitter(Emitter);

	if (bHasContact)
	{
		// 같은 발신기가 반복해서 알려도 한 번만 등록
		ActiveEmitters.AddUnique(WeakEmitter);
	}
	else
	{
		ActiveEmitters.Remove(WeakEmitter);
	}

	UpdateReceiverState();
}

void ALaserReceiver::UpdateReceiverState()
{
	// 파괴되었거나 사용할 수 없는 발신기 제거
	ActiveEmitters.RemoveAll([](const TWeakObjectPtr<ALaserEmitter>& WeakEmitter)
		{
			return !WeakEmitter.IsValid();
		}
	);

	const bool bHasLaserContact = !ActiveEmitters.IsEmpty();

	// 사용 가능한 수신기에 레이저가 하나 이상 들어오면 활성화
	const bool bShouldBeActive = bIsEnabled && bHasLaserContact;

	ApplyActiveState(bShouldBeActive);

	// 발신기가 등록되어 있는 동안에만 소멸 여부 확인
	SetActorTickEnabled(bHasLaserContact);
}