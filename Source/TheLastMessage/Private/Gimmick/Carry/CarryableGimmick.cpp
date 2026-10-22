// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Carry/CarryableGimmick.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/Component/CarryComponent.h"
#include "GameplayEffect.h"
#include "PhysicsEngine/BodyInstance.h"

ACarryableGimmick::ACarryableGimmick()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // 물리 이동이 끝난 뒤 위치 보정
    PrimaryActorTick.TickGroup = TG_PostPhysics;

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

    if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDead())
    {
        return false;
    }

    const UCarryComponent* CarryComponent = PlayerCharacter->FindComponentByClass<UCarryComponent>();

    return IsValid(CarryComponent) && !CarryComponent->HasCarriedObject();
}

bool ACarryableGimmick::CanInteractInternal(ACharacter* Player) const
{
    return CanBePickedUp(Player);
}

void ACarryableGimmick::PerformInteraction(ACharacter* Player)
{
    if (!CanInteractInternal(Player))
    {
        return;
    }

    UCarryComponent* CarryComponent = Player->FindComponentByClass<UCarryComponent>();

    if (!IsValid(CarryComponent))
    {
        return;
    }

    CarryComponent->TryPickUp(this);
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

    // 내려놓은 뒤 물리 시뮬레이션의 Y축 제한 다시 적용
    ApplyYLock();

    UpdateInteractionHighlight(false);
}

FVector ACarryableGimmick::ConstrainDropLocation(const FVector& WorldLocation) const
{
    FVector Result = WorldLocation;

    if (bHasInitialWorldY)
    {
        Result.Y = InitialWorldY;
    }

    return Result;
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
    // 부모의 시작 처리 전에 최초 월드 Y 좌표 기록
    InitialWorldY = GetActorLocation().Y;
    bHasInitialWorldY = true;

    Super::BeginPlay();

    LoadCarryConfig();

    ApplyYLock();
    SetActorTickEnabled(true);
}

void ACarryableGimmick::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateYLockedLocation();
}

void ACarryableGimmick::ApplyYLock()
{
    if (!bHasInitialWorldY || !IsValid(ObjectMesh))
    {
        return;
    }

    UpdateYLockedLocation();

    // 물리 시뮬레이션 중 월드 Y축 방향 이동 제한
    ObjectMesh->BodyInstance.bLockYTranslation = true;
    ObjectMesh->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
}

void ACarryableGimmick::UpdateYLockedLocation()
{
    // 기준 좌표가 없거나 플레이어가 들고 있으면 보정하지 않음
    if (!bHasInitialWorldY || Carrier.IsValid())
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();

    // 들고 있지 않을 때만 기준 Y 좌표로 보정
    const FVector DesiredLocation = ConstrainDropLocation(CurrentLocation);

    if (!CurrentLocation.Equals(DesiredLocation, 0.01))
    {
        SetActorLocation(DesiredLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}
