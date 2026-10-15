// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/GimmickBase.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"

// Sets default values
AGimmickBase::AGimmickBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);

	InteractionEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("InteractionEffect"));

	InteractionEffect->SetupAttachment(SceneRoot);
	InteractionEffect->SetAutoActivate(false);
}

bool AGimmickBase::CanInteractInternal(
	ACharacter* Player) const
{
	return bIsEnabled
		&& bSupportsInteraction
		&& IsValid(Player);
}

void AGimmickBase::PerformInteraction(
	ACharacter* Player)
{
	if (!CanInteractInternal(Player))
	{
		return;
	}

	// 부모 기믹에는 고유 상호작용 동작이 없음.
	// 스위치 등 자식 C++ 클래스에서 실제 기능을 구현.
}

void AGimmickBase::ApplyActiveState(bool bNewActive)
{
	// 같은 상태의 반복 처리 방지
	if (bIsActive == bNewActive)
	{
		return;
	}

	bIsActive = bNewActive;

	// 스위치 그룹 등 상태를 관찰하는 대상에 알림
	OnActiveStateChanged.Broadcast(this, bNewActive);

	// 연결 장치의 BPI 호출은 Blueprint에서 수행
	NotifyLinkedDevices(bNewActive);
}

void AGimmickBase::UpdateInteractionHighlight(
	bool bVisible)
{
	const bool bShowHighlight =
		bVisible && bIsEnabled && bSupportsInteraction;

	bIsHighlighted = bShowHighlight;

	const float TargetIntensity =
		bShowHighlight ? GlowIntensity : 0.0f;

	for (UMaterialInstanceDynamic* GlowMaterial : GlowMaterials)
	{
		if (!IsValid(GlowMaterial))
		{
			continue;
		}

		GlowMaterial->SetScalarParameterValue(
			GlowParameterName,
			TargetIntensity);
	}

	if (!IsValid(InteractionEffect))
	{
		return;
	}

	if (bShowHighlight && InteractionEffect->GetAsset())
	{
		// 반복 갱신 때 효과를 처음부터 재생하지 않도록 처리
		if (!InteractionEffect->IsActive())
		{
			InteractionEffect->Activate(true);
		}
	}
	else
	{
		InteractionEffect->Deactivate();
	}
}

bool AGimmickBase::ApplyConfigOverride(FName ConfigSlot, const FDataTableRowHandle& Row)
{
	// 부모는 특정 설정 슬롯을 처리하지 않음.
	// 자식 클래스가 슬롯과 행을 검증하고 적용한 경우에만
	// true를 반환하도록 재정의.
	return false;
}

// Called when the game starts or when spawned
void AGimmickBase::BeginPlay()
{
	Super::BeginPlay();
	
	// 자식 Blueprint에 추가된 메시 컴포넌트까지 수집
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);

	GlowMaterials.Reset();

	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();

		for (int32 MaterialIndex = 0;
			MaterialIndex < MaterialCount;
			++MaterialIndex)
		{
			if (!MeshComponent->GetMaterial(MaterialIndex))
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex);

			if (IsValid(DynamicMaterial))
			{
				GlowMaterials.AddUnique(DynamicMaterial);
			}
		}
	}

	// 시작할 때 강조 표시 해제
	UpdateInteractionHighlight(false);
}

// Called every frame
void AGimmickBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
