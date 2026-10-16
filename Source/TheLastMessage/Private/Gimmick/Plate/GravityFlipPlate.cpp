// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Plate/GravityFlipPlate.h"

#include "Character/Player/PlayerBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Gimmick/Component/GravityStateComponent.h"

AGravityFlipPlate::AGravityFlipPlate()
{
	// 진입 감지로 작동하는 발판
	bSupportsInteraction = false;

	PlateTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PlateTrigger"));
	PlateTrigger->SetupAttachment(SceneRoot);
	PlateTrigger->SetBoxExtent(FVector(100.0f, 100.0f, 30.0f));
	PlateTrigger->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
	PlateTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PlateTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	PlateTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	PlateTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PlateTrigger->SetGenerateOverlapEvents(true);
}

void AGravityFlipPlate::BeginPlay()
{
	Super::BeginPlay();

	PlateTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AGravityFlipPlate::OnPlateBeginOverlap);
	PlateTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &AGravityFlipPlate::OnPlateEndOverlap);
}

void AGravityFlipPlate::HandlePlateEnter(ACharacter* Player)
{
	APlayerBase* PlayerCharacter = Cast<APlayerBase>(Player);

	if (!bIsEnabled || !HasAuthority() || !IsValid(PlayerCharacter) || PlayerCharacter->IsDead())
	{
		return;
	}

	// 소멸한 플레이어 참조 정리
	OverlappingPlayers.RemoveAll( [](const TWeakObjectPtr<ACharacter>& Entry)
		{
			return !Entry.IsValid();
		});

	const TWeakObjectPtr<ACharacter> PlayerReference(PlayerCharacter);

	// 이미 안에 있는 플레이어는 다시 반전하지 않음
	if (OverlappingPlayers.Contains(PlayerReference))
	{
		return;
	}

	UGravityStateComponent* GravityComponent = PlayerCharacter->FindComponentByClass<UGravityStateComponent>();

	if (!IsValid(GravityComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 진입한 플레이어에 GravityStateComponent가 없습니다."), *GetName());

		return;
	}

	// 반전으로 이동이 발생하기 전에 진입 상태부터 기록
	OverlappingPlayers.Add(PlayerReference);

	GravityComponent->ToggleGravityState();
}

void AGravityFlipPlate::HandlePlateExit(ACharacter* Player)
{
	// 이탈 시 목록만 정리하고 중력 상태는 유지
	OverlappingPlayers.RemoveAll([Player](const TWeakObjectPtr<ACharacter>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Player;
		});
}

void AGravityFlipPlate::OnPlateBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);

	if (!IsValid(Player))
	{
		return;
	}

	// 캐릭터의 메시 등 다른 컴포넌트가 만드는 중복 진입 제외
	if (OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	HandlePlateEnter(Player);
}

void AGravityFlipPlate::OnPlateEndOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	APlayerBase* Player = Cast<APlayerBase>(OtherActor);

	if (!IsValid(Player))
	{
		return;
	}

	if (OtherComp != Player->GetCapsuleComponent())
	{
		return;
	}

	HandlePlateExit(Player);
}

void AGravityFlipPlate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(PlateTrigger))
	{
		PlateTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &AGravityFlipPlate::OnPlateBeginOverlap);
		PlateTrigger->OnComponentEndOverlap.RemoveDynamic(this, &AGravityFlipPlate::OnPlateEndOverlap);
	}

	OverlappingPlayers.Reset();

	Super::EndPlay(EndPlayReason);
}