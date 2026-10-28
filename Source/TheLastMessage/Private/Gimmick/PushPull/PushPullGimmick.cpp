// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/PushPull/PushPullGimmick.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/Component/PushPullComponent.h"
#include "PhysicsEngine/BodyInstance.h"

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

    SetPushPullMovementEnabled(true);

    UpdateInteractionHighlight(false);
}

void APushPullGimmick::EndPushPull()
{
    ControllingPlayer.Reset();

    SetPushPullMovementEnabled(false);

    // 목표 위치가 아닌 물리 메시의 실제 배치 상태 기록
    if (IsValid(PhysicsMesh))
    {
        LastPlacedWorldTransform = PhysicsMesh->GetComponentTransform();
    }
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
    // 게임 시작 시 최초 배치 상태 기록
    InitialWorldTransform = GetActorTransform();

    // 아직 이동하지 않았으므로 최초 배치 상태로 초기화
    LastPlacedWorldTransform = InitialWorldTransform;

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

    // 처음에는 상호작용 없이 수평으로 밀리지 않도록 잠금
    SetPushPullMovementEnabled(false);
}

void APushPullGimmick::SetPushPullMovementEnabled(bool bAllowHorizontalMovement)
{
    if (!IsValid(PhysicsMesh) || !PhysicsMesh->IsSimulatingPhysics())
    {
        return;
    }

    FBodyInstance& Body = PhysicsMesh->BodyInstance;

    // 조작 중에만 좌우 이동 허용
    Body.bLockXTranslation = !bAllowHorizontalMovement;

    // 이동 평면을 벗어나지 않도록 깊이 방향 고정
    Body.bLockYTranslation = true;

    // 중력에 의한 낙하는 유지
    Body.bLockZTranslation = false;

    // 몸으로 부딪혔을 때 굴러가거나 넘어지는 현상 방지
    Body.bLockXRotation = true;
    Body.bLockYRotation = true;
    Body.bLockZRotation = true;

    // 변경한 축별 잠금 상태를 실제 물리에 적용
    Body.SetDOFLock(EDOFMode::SixDOF);

    // 조작 종료 시 남아 있는 수평 속도 제거
    FVector Velocity = PhysicsMesh->GetPhysicsLinearVelocity();

    if (!bAllowHorizontalMovement)
    {
        Velocity.X = 0.0;
    }

    Velocity.Y = 0.0;

    // Z축 속도를 유지해서 떨어지는 도중에도 낙하가 계속되도록 처리
    PhysicsMesh->SetPhysicsLinearVelocity(Velocity);
    PhysicsMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

    PhysicsMesh->WakeAllRigidBodies();
}


FProgressActorState APushPullGimmick::CaptureProgress() const
{
    FProgressActorState State = Super::CaptureProgress();
    State.Transform = IsValid(PhysicsMesh) ? PhysicsMesh->GetComponentTransform() : LastPlacedWorldTransform;
    return State;
}

void APushPullGimmick::RestoreProgress(const FProgressActorState& State)
{
    ControllingPlayer.Reset();
    if (IsValid(PhysicsMesh))
    {
        PhysicsMesh->SetWorldTransform(State.Transform, false, nullptr, ETeleportType::TeleportPhysics);
        if (PhysicsMesh->IsSimulatingPhysics())
        {
            PhysicsMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            PhysicsMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
    }
    LastPlacedWorldTransform = State.Transform;
    SetPushPullMovementEnabled(false);
    Super::RestoreProgress(State);
}
