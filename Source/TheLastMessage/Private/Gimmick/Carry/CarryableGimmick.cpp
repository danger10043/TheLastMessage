// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Carry/CarryableGimmick.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gimmick/Component/CarryComponent.h"
#include "GameplayEffect.h"
#include "PhysicsEngine/BodyInstance.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"

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
    if (ReservedCarrier.IsValid() && ReservedCarrier.Get() != Player) return false;
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

bool ACarryableGimmick::ReserveForPickup(ACharacter* Player)
{
    if (!CanBePickedUp(Player)) return false;
    ReservedCarrier = Player;
    return true;
}

void ACarryableGimmick::ReleasePickupReservation(ACharacter* Player)
{
    if (ReservedCarrier.Get() == Player) ReservedCarrier.Reset();
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

    // 떨어지는 중에 다시 집으면 낙하 중단
    bIsDropFalling = false;
    DropFallSpeed = 0.0f;
    DropIgnoredPlayer.Reset();

    Carrier = Player;

    // 운반을 시작하면 일반 위치 관리 재개
    SetActorTickEnabled(true);

    // 집은 물체의 상호작용 강조 해제
    UpdateInteractionHighlight(false);
}

void ACarryableGimmick::OnDropped()
{
    // 운반자 참조를 지우기 전에 낙하 검사에서 제외할 플레이어 기록
    DropIgnoredPlayer = Carrier;

    Carrier.Reset();

    DropFallSpeed = 0.0f;
    bIsDropFalling = false;

    if (IsValid(ObjectMesh))
    {
        // 물리 힘으로 이동하지 않고 코드에서 직접 낙하 처리
        ObjectMesh->SetSimulatePhysics(false);
        ObjectMesh->SetEnableGravity(false);

        // 충돌 검사는 유지하지만 물리 시뮬레이션에는 참여하지 않음
        ObjectMesh->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly);

        // 기존 규칙대로 들기 전 Y 좌표 복구
        ApplyYLock();

        bIsDropFalling = true;
        SetActorTickEnabled(true);
    }

    UpdateInteractionHighlight(false);
}

void ACarryableGimmick::OnTransferred()
{
    bIsDropFalling = false;
    DropFallSpeed = 0.0f;
    DropIgnoredPlayer.Reset();

    Carrier.Reset();

    // 장착 후에는 최초 Y 좌표로 되돌리는 갱신 중지
    SetActorTickEnabled(false);

    if (IsValid(ObjectMesh))
    {
        // 장착 위치를 유지하도록 기존 Y축 물리 제한 해제
        ObjectMesh->BodyInstance.bLockYTranslation = false;
        ObjectMesh->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
    }

    // 회로에 부착된 월드 배치 상태 기록
    LastPlacedWorldTransform = GetActorTransform();

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
    // 게임 시작 시 최초 배치 상태 기록
    InitialWorldTransform = GetActorTransform();

    // 아직 이동하지 않았으므로 최초 배치 상태로 초기화
    LastPlacedWorldTransform = InitialWorldTransform;

    // 부모의 시작 처리 전에 최초 월드 Y 좌표 기록
    InitialWorldY = GetActorLocation().Y;
    bHasInitialWorldY = true;

    Super::BeginPlay();

    if (IsValid(ObjectMesh))
    {
        ObjectMesh->SetSimulatePhysics(false);
        ObjectMesh->SetEnableGravity(false);
        ObjectMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    LoadCarryConfig();

    ApplyYLock();
    SetActorTickEnabled(true);
}

void ACarryableGimmick::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateYLockedLocation();

    UpdateDropFall(DeltaSeconds);
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

void ACarryableGimmick::UpdateDropFall(float DeltaSeconds)
{
    if (!bIsDropFalling || Carrier.IsValid() || !HasAuthority() || DeltaSeconds <= 0.0f)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World || !IsValid(ObjectMesh) || !ObjectMesh->GetStaticMesh())
    {
        bIsDropFalling = false;
        DropFallSpeed = 0.0f;
        DropIgnoredPlayer.Reset();
        return;
    }

    // 플레이어의 중력 반전과 관계없이 월드 아래쪽으로 낙하
    const float FallAcceleration = FMath::Abs(World->GetGravityZ());

    constexpr float MaxFallSpeed = 1500.0f;

    const float PreviousSpeed = DropFallSpeed;

    DropFallSpeed = FMath::Min(DropFallSpeed + FallAcceleration * DeltaSeconds, MaxFallSpeed);

    // 이전 속도와 현재 속도의 평균으로 이번 프레임 이동 거리 계산
    const double FallDistance = (PreviousSpeed + DropFallSpeed) * 0.5 * DeltaSeconds;

    if (FallDistance <= 0.0)
    {
        return;
    }

    const FVector FallDirection = FVector::DownVector;
    const FVector MoveDelta = FallDirection * FallDistance;

    // 메시 피벗이 중앙에 있지 않은 경우도 고려
    FVector LocalMin;
    FVector LocalMax;
    ObjectMesh->GetLocalBounds(LocalMin, LocalMax);

    const FVector LocalCenter = (LocalMin + LocalMax) * 0.5;

    const FVector LocalHalfSize = (LocalMax - LocalMin) * 0.5;

    const FTransform MeshTransform = ObjectMesh->GetComponentTransform();

    const FVector ScaledHalfSize = LocalHalfSize * MeshTransform.GetScale3D().GetAbs();

    const FVector HalfSize(FMath::Max(ScaledHalfSize.X, 1.0), FMath::Max(ScaledHalfSize.Y, 1.0), FMath::Max(ScaledHalfSize.Z, 1.0));

    const FVector StartCenter = MeshTransform.TransformPosition(LocalCenter);

    const FVector EndCenter = StartCenter + MoveDelta;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CarryItemDropFall), false, this);

    // 내려놓은 플레이어를 바닥으로 판단하지 않도록 제외
    if (ACharacter* IgnoredPlayer = DropIgnoredPlayer.Get())
    {
        QueryParams.AddIgnoredActor(IgnoredPlayer);
    }

    FHitResult Hit;

    const bool bBlocked = World->SweepSingleByChannel(
        Hit,
        StartCenter,
        EndCenter,
        MeshTransform.GetRotation(),
        ECC_WorldDynamic,
        FCollisionShape::MakeBox(HalfSize),
        QueryParams);

    if (bBlocked)
    {
        // 처음부터 장애물 안에 겹쳐 있으면 관통 이동 방지
        if (Hit.bStartPenetrating)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: 낙하 시작 위치가 장애물과 겹쳐 있습니다."), *GetName());
        }
        else
        {
            // 충돌 지점보다 아주 조금 앞에서 멈춤
            constexpr double SurfaceClearance = 0.5;

            const double AllowedDistance = FMath::Max(0.0, FallDistance * Hit.Time - SurfaceClearance);

            SetActorLocation(
                GetActorLocation()
                + FallDirection * AllowedDistance,
                false,
                nullptr,
                ETeleportType::TeleportPhysics);

            // 정상적으로 착지한 월드 배치 상태 기록
            LastPlacedWorldTransform = GetActorTransform();
        }

        // 착지하면 현재 위치에 고정
        bIsDropFalling = false;
        DropFallSpeed = 0.0f;
        DropIgnoredPlayer.Reset();
        return;
    }

    // 장애물이 없으면 X와 Y는 유지하고 아래로만 이동
    SetActorLocation(GetActorLocation() + MoveDelta, false, nullptr, ETeleportType::TeleportPhysics);

}
