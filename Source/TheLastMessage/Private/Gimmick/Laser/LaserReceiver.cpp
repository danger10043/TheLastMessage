// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Laser/LaserReceiver.h"


#include "Components/StaticMeshComponent.h"
#include "Gimmick/Laser/LaserEmitter.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

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

	// 입구 판정 범위는 블루프린트에서 조절
	ReceiverInputArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ReceiverInputArea"));

	ReceiverInputArea->SetupAttachment(ReceiverMesh);
	ReceiverInputArea->SetMobility(EComponentMobility::Movable);

	ReceiverInputArea->SetBoxExtent(FVector(5.0f, 20.0f, 20.0f));

	// 박스 자체는 레이저나 플레이어를 막지 않음
	ReceiverInputArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ReceiverInputArea->SetCollisionResponseToAllChannels(ECR_Ignore);

	ReceiverInputArea->SetGenerateOverlapEvents(false);
	ReceiverInputArea->SetCanEverAffectNavigation(false);
	ReceiverInputArea->SetHiddenInGame(true);
}

void ALaserReceiver::ApplyActiveState(bool bNewActive)
{
	const bool bPreviousActive = bIsActive;

	Super::ApplyActiveState(bNewActive && bIsEnabled);

	// 상태가 바뀌었을 때만 표시등 갱신
	if (bPreviousActive != bIsActive)
	{
		UpdateReceiverIndicator();
	}
}

bool ALaserReceiver::CanReceiveLaserHit(const FHitResult& Hit) const
{
	if (!bIsEnabled || !IsValid(ReceiverMesh) || !IsValid(ReceiverInputArea))
	{
		return false;
	}

	// 수신기 몸체 메시와 부딪힌 결과만 사용
	if (Hit.GetComponent() != ReceiverMesh.Get())
	{
		return false;
	}

	// 충돌 위치를 입구 판정 박스의 로컬 좌표로 변환
	const FVector LocalHit = ReceiverInputArea->GetComponentTransform() .InverseTransformPosition(Hit.ImpactPoint);

	const FVector Extent = ReceiverInputArea->GetUnscaledBoxExtent();

	// 충돌 위치가 박스 안에 들어왔는지 확인
	return FMath::Abs(LocalHit.X) <= Extent.X && FMath::Abs(LocalHit.Y) <= Extent.Y && FMath::Abs(LocalHit.Z) <= Extent.Z;
}

void ALaserReceiver::BeginPlay()
{
	Super::BeginPlay();

	ReceiverIndicatorMaterial = Cast<UMaterialInstanceDynamic>(ReceiverMesh->GetMaterial(IndicatorMaterialIndex));

	if (!IsValid(ReceiverIndicatorMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ReceiverMesh의 표시등 머티리얼 슬롯을 확인해주세요."), *GetName());
	}

	UpdateReceiverState();

	// 초기 활성 상태와 관계없이 표시등 색상을 한 번 반영
	UpdateReceiverIndicator();
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

bool ALaserReceiver::HasLaserContactFrom(const ALaserEmitter* Emitter) const
{
	if (!bIsEnabled || !IsValid(Emitter)
		|| !Emitter->IsGimmickEnabled() || !Emitter->IsGimmickActive())
	{
		return false;
	}

	return ActiveEmitters.ContainsByPredicate(
		[Emitter](const TWeakObjectPtr<ALaserEmitter>& Entry)
		{
			return Entry.Get() == Emitter;
		});
}

void ALaserReceiver::UpdateReceiverIndicator()
{
	if (!IsValid(ReceiverIndicatorMaterial))
	{
		return;
	}

	const FLinearColor LightColor = bIsEnabled && bIsActive ? FLinearColor::Green : FLinearColor::Black;

	ReceiverIndicatorMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LightColor);
}