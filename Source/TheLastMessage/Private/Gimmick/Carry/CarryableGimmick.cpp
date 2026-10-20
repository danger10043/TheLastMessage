// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Carry/CarryableGimmick.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"

ACarryableGimmick::ACarryableGimmick()
{
    bSupportsInteraction = true;

    ObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObjectMesh"));

    ObjectMesh->SetMobility(EComponentMobility::Movable);
    ObjectMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ObjectMesh->SetGenerateOverlapEvents(false);
    ObjectMesh->SetSimulatePhysics(false);

    // 물리 이동 시 메시와 액터 위치가 함께 움직이도록 메시를 루트로 사용
    SetRootComponent(ObjectMesh);

    // 부모의 기준 컴포넌트와 상호작용 효과도 물체를 따라가도록 연결
    SceneRoot->SetMobility(EComponentMobility::Movable);
    SceneRoot->SetupAttachment(ObjectMesh);
}

bool ACarryableGimmick::LoadCarryConfig()
{
    bCarryConfigLoaded = false;

    if (!CarryConfigRow.DataTable || CarryConfigRow.RowName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: CarryConfigRow의 테이블과 행을 지정해주세요."), *GetName());

        return false;
    }

    const FCarryConfigRow* LoadedRow = CarryConfigRow.GetRow<FCarryConfigRow>(TEXT("ACarryableGimmick::LoadCarryConfig"));

    if (!LoadedRow)
    {
        return false;
    }

    if (!FMath::IsFinite(LoadedRow->DropDistance) || LoadedRow->DropDistance <= 0.0f || LoadedRow->CarryOffset.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 내려놓을 거리는 0보다 커야 하며 운반 Transform이 유효해야 합니다."), *GetName());

        return false;
    }

    if (!LoadedRow->CarryEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 운반 Gameplay Effect를 지정해주세요."), *GetName());

        return false;
    }

    const UGameplayEffect* EffectDefault = LoadedRow->CarryEffectClass.GetDefaultObject();

    if (!IsValid(EffectDefault) || EffectDefault->DurationPolicy != EGameplayEffectDurationType::Infinite)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 운반 Gameplay Effect는 Infinite여야 합니다."), *GetName());

        return false;
    }

    CarryConfig = *LoadedRow;
    bCarryConfigLoaded = true;

    return true;
}

bool ACarryableGimmick::CanBePickedUp(ACharacter* Player) const
{
    if (!Super::CanInteractInternal(Player) || !bCarryConfigLoaded || !IsValid(ObjectMesh) || !ObjectMesh->GetStaticMesh() || Carrier.IsValid())
    {
        return false;
    }

    const APlayerBase* PlayerCharacter = Cast<APlayerBase>(Player);

    return IsValid(PlayerCharacter) && !PlayerCharacter->IsDead();
}

bool ACarryableGimmick::CanInteractInternal(ACharacter* Player) const
{
    return CanBePickedUp(Player);
}

void ACarryableGimmick::OnPickedUp(ACharacter* Player)
{
    if (!CanBePickedUp(Player))
    {
        return;
    }

    Carrier = Player;

    // 집은 물체의 상호작용 강조 해제
    UpdateInteractionHighlight(false);
}

void ACarryableGimmick::OnDropped()
{
    Carrier.Reset();

    UpdateInteractionHighlight(false);
}

UStaticMeshComponent* ACarryableGimmick::GetObjectMesh() const
{
    return ObjectMesh.Get();
}

ACharacter* ACarryableGimmick::GetCarrier() const
{
    return Carrier.Get();
}

void ACarryableGimmick::BeginPlay()
{
    Super::BeginPlay();

    LoadCarryConfig();
}
