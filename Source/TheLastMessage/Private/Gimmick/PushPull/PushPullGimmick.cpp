// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/PushPull/PushPullGimmick.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/Component/PushPullComponent.h"

APushPullGimmick::APushPullGimmick()
{
    // 실제 조작 갱신은 플레이어의 PushPullComponent에서 처리
    PrimaryActorTick.bCanEverTick = false;

    bSupportsInteraction = true;

    PhysicsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsMesh"));

    PhysicsMesh->SetMobility(EComponentMobility::Movable);
    PhysicsMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    PhysicsMesh->SetGenerateOverlapEvents(false);

    // 메시가 지정된 이후 BeginPlay에서 물리 시뮬레이션 시작
    PhysicsMesh->SetSimulatePhysics(false);

    // 물리 이동 시 액터 위치도 함께 이동하도록 메시를 루트로 사용
    SetRootComponent(PhysicsMesh);

    // 부모의 기준점과 효과도 물체를 따라가도록 연결
    SceneRoot->SetMobility(EComponentMobility::Movable);
    SceneRoot->SetupAttachment(PhysicsMesh);

    InteractionAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionAnchor"));

    InteractionAnchor->SetMobility(EComponentMobility::Movable);
    InteractionAnchor->SetupAttachment(PhysicsMesh);
}

bool APushPullGimmick::LoadPushPullConfig()
{
    bHasValidPushPullConfig = false;

    if (!PushPullConfigRow.DataTable || PushPullConfigRow.RowName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: PushPullConfigRow의 테이블과 행을 지정해주세요."), *GetName());

        return false;
    }

    const FPushPullConfigRow* LoadedRow = PushPullConfigRow.GetRow<FPushPullConfigRow>(TEXT("APushPullGimmick::LoadPushPullConfig"));

    if (!LoadedRow)
    {
        return false;
    }

    if (!FMath::IsFinite(LoadedRow->MoveSpeed) 
        || LoadedRow->MoveSpeed <= 0.0f 
        || !FMath::IsFinite(LoadedRow->MaxSeparationDistance)
        || LoadedRow->MaxSeparationDistance <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: MoveSpeed와 MaxSeparationDistance는 유효한 양수여야 합니다."), *GetName());

        return false;
    }

    PushPullConfig = *LoadedRow;
    bHasValidPushPullConfig = true;

    return true;
}

bool APushPullGimmick::CanBeMovedBy(ACharacter* Player) const
{
    if (!Super::CanInteractInternal(Player) || !bCanBeMoved || !bHasValidPushPullConfig || ControllingPlayer.IsValid())
    {
        return false;
    }

    const APlayerBase* PlayerCharacter = Cast<APlayerBase>(Player);

    if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDead())
    {
        return false;
    }

    if (!IsValid(PhysicsMesh) 
        || !PhysicsMesh->GetStaticMesh() 
        || !PhysicsMesh->IsSimulatingPhysics() 
        || PhysicsMesh->GetCollisionEnabled()
        == ECollisionEnabled::NoCollision)
    {
        return false;
    }

    return true;
}

bool APushPullGimmick::CanInteractInternal(ACharacter* Player) const
{
    return CanBeMovedBy(Player);
}

void APushPullGimmick::PerformInteraction(ACharacter* Player)
{
    if (!CanInteractInternal(Player))
    {
        return;
    }

    UPushPullComponent* PushPullComponent = Player->FindComponentByClass<UPushPullComponent>();

    if (!IsValid(PushPullComponent))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 플레이어에 PushPullComponent가 없습니다."), *GetName());

        return;
    }

    PushPullComponent->StartPushPull(this);
}

void APushPullGimmick::BeginPushPull(ACharacter* Player)
{
    if (!CanBeMovedBy(Player))
    {
        return;
    }

    ControllingPlayer = Player;

    UpdateInteractionHighlight(false);
}

void APushPullGimmick::EndPushPull()
{
    ControllingPlayer.Reset();
}

UStaticMeshComponent* APushPullGimmick::GetPhysicsMesh() const
{
    return PhysicsMesh.Get();
}

USceneComponent* APushPullGimmick::GetInteractionAnchor() const
{
    return InteractionAnchor.Get();
}

ACharacter* APushPullGimmick::GetControllingPlayer() const
{
    return ControllingPlayer.Get();
}

void APushPullGimmick::BeginPlay()
{
    Super::BeginPlay();

    LoadPushPullConfig();

    if (!IsValid(PhysicsMesh) || !PhysicsMesh->GetStaticMesh())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: PhysicsMesh에 Static Mesh를 지정해주세요."), *GetName());

        return;
    }

    // 무게 발판 등이 이 물체의 진입과 이탈을 감지하도록 설정
    PhysicsMesh->SetGenerateOverlapEvents(true);

    PhysicsMesh->SetEnableGravity(true);
    PhysicsMesh->SetSimulatePhysics(true);

    if (!PhysicsMesh->IsSimulatingPhysics())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 물리 시뮬레이션을 시작하지 못했습니다. 메시의 단순 충돌과 충돌 설정을 확인해주세요."), *GetName());
    }
}

