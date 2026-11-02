// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/PushPullComponent.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/CarryComponent.h"
#include "Gimmick/PushPull/PushPullGimmick.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"

UPushPullComponent::UPushPullComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 실제 조작 중에만 갱신
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

bool UPushPullComponent::StartPushPull(APushPullGimmick* Target)
{
    // 이미 다른 물체를 조작 중이면 새 조작을 시작하지 않음
    if (CurrentTarget.IsValid())
    {
        return false;
    }

    // 이전 대상이 사라졌을 때 남을 수 있는 상태 정리
    StopPushPull();

    APlayerBase* Player = OwnerPlayer.Get();

    // 공격·차지 중에는 조작을 시작하지 않는다.
    if (!IsValid(Player) || Player->IsDead() || Player->IsHitReacting() || Player->IsAttacking()
        || Player->IsCarryTransitionActive() || !IsValid(Target) /* || !IsValid(PhysicsHandle)*/)
    {
        return false;
    }

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    // 지면에 서 있을 때만 조작 시작
    if (!IsValid(Movement) || !Movement->IsMovingOnGround())
    {
        return false;
    }

    const UCarryComponent* CarryComponent = Player->FindComponentByClass<UCarryComponent>();

    // 다른 물체를 들고 있으면 조작하지 않음
    if (IsValid(CarryComponent) && CarryComponent->HasCarriedObject())
    {
        return false;
    }

    if (!Target->CanBeMovedBy(Player))
    {
        return false;
    }

    UStaticMeshComponent* Mesh = Target->GetPhysicsMesh();
    USceneComponent* Anchor = Target->GetInteractionAnchor();

    if (!IsValid(Mesh) || !IsValid(Anchor))
    {
        return false;
    }

    const FPushPullConfigRow& Config = Target->GetPushPullConfig();

    // 조작 기준점에서 너무 멀리 떨어져 있으면 시작하지 않음
    const float DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), Anchor->GetComponentLocation());

    if (DistanceSquared > FMath::Square(Config.MaxSeparationDistance))
    {
        return false;
    }

    ActivePlaneNormal = MovementPlaneNormal.GetSafeNormal();

    if (Movement->bConstrainToPlane)
    {
        const FVector PlayerPlaneNormal = Movement->GetPlaneConstraintNormal().GetSafeNormal();

        if (!PlayerPlaneNormal.IsNearlyZero())
        {
            ActivePlaneNormal = PlayerPlaneNormal;
        }
    }

    if (ActivePlaneNormal.IsNearlyZero())
    {
        return false;
    }

    // 물리 핸들 없이 물체의 수평 이동 잠금 해제
    Target->BeginPushPull(Player);

    if (!IsValid(Target) || Target->GetControllingPlayer() != Player)
    {
        return false;
    }

    CurrentTarget = Target;

    // 현재 잡은 위치를 기준으로 좌우 간격 저장
    InitialRelativeOffsetX = Mesh->GetComponentLocation().X - Player->GetActorLocation().X;

    // 이전 조작의 입력과 속도가 남지 않도록 초기화
    PendingMoveInput = 0.0f;
    SharedMoveSpeedX = 0.0;

    // 조작 중 빠르게 움직일 때 충돌 누락을 줄이도록 설정
    Mesh->SetUseCCD(true);

    // 조작 중에는 물체를 바라보는 방향 유지
    bOriginalOrientToMovement = Movement->bOrientRotationToMovement;
    Movement->bOrientRotationToMovement = false;

    const FVector Facing = FVector::VectorPlaneProject(
        Mesh->GetComponentLocation() - Player->GetActorLocation(),
        Player->GetActorUpVector()).GetSafeNormal();

    if (!Facing.IsNearlyZero())
    {
        Player->SetActorRotation(FRotationMatrix::MakeFromXZ(Facing, Player->GetActorUpVector()).Rotator());
    }

    // 조작 전 걷기 속도를 기록하고 제한 적용
    OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
    AppliedMaxWalkSpeed = OriginalMaxWalkSpeed;
    bHasWalkSpeedOverride = true;

    UpdateWalkSpeedLimit(Movement, Config.MoveSpeed);

    SetComponentTickEnabled(true);

    return true;
}

void UPushPullComponent::UpdatePushPull(const FVector& MoveDirection, float DeltaTime)
{
    APlayerBase* Player = OwnerPlayer.Get();
    APushPullGimmick* Target = CurrentTarget.Get();

    if (!IsValid(Player) 
        || Player->IsDead() 
        || Player->IsHitReacting() 
        || !IsValid(Target) 
        || !Target->IsGimmickEnabled() 
        || Target->GetControllingPlayer() != Player)
    {
        StopPushPull();
        return;
    }

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    // 점프하거나 발밑 지면이 없어지면 물체를 놓음
    if (!IsValid(Movement) || !Movement->IsMovingOnGround())
    {
        StopPushPull();
        return;
    }

    const UCarryComponent* CarryComponent = Player->FindComponentByClass<UCarryComponent>();

    if (IsValid(CarryComponent) && CarryComponent->HasCarriedObject())
    {
        StopPushPull();
        return;
    }

    UStaticMeshComponent* Mesh = Target->GetPhysicsMesh();
    USceneComponent* Anchor = Target->GetInteractionAnchor();

    if (!IsValid(Mesh) 
        || !IsValid(Anchor)
        || !Mesh->IsSimulatingPhysics()
        || Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        StopPushPull();
        return;
    }

    const FPushPullConfigRow& Config = Target->GetPushPullConfig();

    // 새 설정값의 유효성 확인
    if (!FMath::IsFinite(Config.HorizontalAcceleration)
        || Config.HorizontalAcceleration <= 0.0f
        || !FMath::IsFinite(Config.MaxSlopeAngle)
        || Config.MaxSlopeAngle < 0.0f
        || Config.MaxSlopeAngle > 80.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 밀기 가속도는 양수, 최대 경사각은 0~80도여야 합니다."), *Target->GetName());

        StopPushPull();
        return;
    }

    const double DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), Anchor->GetComponentLocation());

    if (DistanceSquared > FMath::Square(static_cast<double>(Config.MaxSeparationDistance)))
    {
        StopPushPull();
        return;
    }

    UpdateWalkSpeedLimit(Movement, Config.MoveSpeed);

    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f || MoveDirection.ContainsNaN())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        StopPushPull();
        return;
    }

    const FVector PlanarDirection = FVector::VectorPlaneProject(MoveDirection, ActivePlaneNormal).GetClampedToMaxSize(1.0f);

    const double EffectiveMoveSpeed = FMath::Max(0.0, static_cast<double>(FMath::Min(Config.MoveSpeed, Movement->MaxWalkSpeed)));

    const double Acceleration = static_cast<double>(Config.HorizontalAcceleration);

    // 플레이어 입력으로 제어하는 것은 월드 X축 속도뿐
    double DesiredSpeedX = PlanarDirection.X * EffectiveMoveSpeed;

    const FVector CurrentVelocity = Mesh->GetPhysicsLinearVelocity();

    if (CurrentVelocity.ContainsNaN())
    {
        StopPushPull();
        return;
    }

    // 진행 방향 앞의 벽과 경사면 검사
    if (FMath::Abs(DesiredSpeedX) > 0.01)
    {
        const double DirectionSign = FMath::Sign(DesiredSpeedX);

        const double CurrentForwardSpeed = FMath::Max(0.0, CurrentVelocity.X * DirectionSign);

        const double ProbeSpeed = FMath::Max(FMath::Abs(DesiredSpeedX), CurrentForwardSpeed);

        constexpr double CollisionMargin = 1.0;

        // 현재 속도에서 멈출 거리까지 미리 확인
        const double BrakingDistance = FMath::Square(ProbeSpeed) / (2.0 * Acceleration);

        const double ProbeDistance = FMath::Min(
            ProbeSpeed * DeltaTime
            + BrakingDistance
            + CollisionMargin,
            static_cast<double>(Config.MaxSeparationDistance));

        const FVector MeshLocation = Mesh->GetComponentLocation();

        const FVector ProbeDelta(DirectionSign * ProbeDistance, 0.0, 0.0);

        FComponentQueryParams QueryParams(SCENE_QUERY_STAT(PushPullMovement), Target);

        QueryParams.AddIgnoredActor(Player);
        QueryParams.bIgnoreTouches = true;

        TArray<FHitResult> SweepHits;

        // 실제 물체의 충돌 형상으로 경로 검사
        World->ComponentSweepMulti(
            SweepHits,
            Mesh,
            MeshLocation,
            MeshLocation + ProbeDelta,
            Mesh->GetComponentQuat(),
            QueryParams);

        // 이 값 이상의 위쪽 법선을 가지면 허용 경사면
        const double MinFloorNormalZ = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(Config.MaxSlopeAngle)));

        for (const FHitResult& Hit : SweepHits)
        {
            if (!Hit.bBlockingHit)
            {
                continue;
            }

            FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();

            if (SurfaceNormal.IsNearlyZero())
            {
                SurfaceNormal = Hit.Normal.GetSafeNormal();
            }

            // 접촉면에서 멀어지거나 나란히 움직이는 경우 허용
            if (FVector::DotProduct(ProbeDelta, SurfaceNormal) >= -UE_KINDA_SMALL_NUMBER)
            {
                continue;
            }

            // 올라갈 수 있는 경사면은 벽으로 취급하지 않음
            if (SurfaceNormal.Z + KINDA_SMALL_NUMBER >= MinFloorNormalZ)
            {
                continue;
            }

            // 벽이나 너무 가파른 경사면 앞에서는 감속
            const double HitDistance = ProbeDistance * FMath::Clamp(static_cast<double>(Hit.Time), 0.0, 1.0);

            // 한 번에 전진할 거리를 작게 제한
            const double StepForwardDistance = FMath::Clamp(FMath::Abs(DesiredSpeedX) * static_cast<double>(DeltaTime) + 2.0, 2.0, 10.0);

            // 가까운 장애물에 대해서만 턱 넘기 시도
            if (HitDistance <= StepForwardDistance && TryStepUp(Mesh, DirectionSign, StepForwardDistance))
            {
                // 위치가 바뀌었으므로 이번 프레임의 나머지 이동 계산 종료
                return;
            }

            const double SafeDistance = FMath::Max(0.0, HitDistance - CollisionMargin);

            const double AllowedSpeed = FMath::Sqrt(2.0 * Acceleration * SafeDistance);

            DesiredSpeedX = DirectionSign * FMath::Min(FMath::Abs(DesiredSpeedX), AllowedSpeed);
        }
    }

    // 목표 수평 속도까지 가속하거나 감속
    const double AccelerationX = FMath::Clamp(
        (DesiredSpeedX - CurrentVelocity.X)
        / static_cast<double>(DeltaTime),
        -Acceleration,
        Acceleration);

    // X축에만 가속도를 적용
    // Z축은 중력과 바닥 충돌에 따라 자유롭게 움직임
    Mesh->AddForce(FVector(AccelerationX, 0.0, 0.0), NAME_None, true);
}

void UPushPullComponent::StopPushPull()
{
    APushPullGimmick* Target = CurrentTarget.Get();

    CurrentTarget.Reset();

    PendingMoveInput = 0.0f;
    InitialRelativeOffsetX = 0.0;
    SharedMoveSpeedX = 0.0;

    SetComponentTickEnabled(false);

    // 물체의 조작 종료 및 수평 이동 잠금
    if (IsValid(Target)
        && Target->GetControllingPlayer() == OwnerPlayer.Get())
    {
        Target->EndPushPull();
    }

    // 컴포넌트가 적용한 회전 설정과 속도 제한 복구
    if (bHasWalkSpeedOverride)
    {
        if (APlayerBase* Player = OwnerPlayer.Get())
        {
            if (UCharacterMovementComponent* Movement =
                Player->GetCharacterMovement())
            {
                Movement->bOrientRotationToMovement =
                    bOriginalOrientToMovement;

                // 다른 기능이 이미 속도를 변경했다면 유지
                if (FMath::IsNearlyEqual(
                    Movement->MaxWalkSpeed,
                    AppliedMaxWalkSpeed))
                {
                    Movement->MaxWalkSpeed = OriginalMaxWalkSpeed;
                }
            }
        }
    }

    bHasWalkSpeedOverride = false;
}

bool UPushPullComponent::IsPushPulling() const
{
    return CurrentTarget.IsValid();
}

APushPullGimmick* UPushPullComponent::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}

void UPushPullComponent::SetMoveInput(float Value)
{
    PendingMoveInput = FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;
}

void UPushPullComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerPlayer = Cast<APlayerBase>(GetOwner());

    APlayerBase* Player = OwnerPlayer.Get();

    if (!IsValid(Player))
    {
        UE_LOG(LogTemp, Warning, TEXT("PushPullComponent는 PlayerBase에 부착해주세요."));

        return;
    }

    // 캐릭터가 이동 입력을 처리한 뒤 물체 제어
    if (UCharacterMovementComponent* Movement =
        Player->GetCharacterMovement())
    {
        AddTickPrerequisiteComponent(Movement);
    }
}

void UPushPullComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopPushPull();

    OwnerPlayer.Reset();

    Super::EndPlay(EndPlayReason);
}

void UPushPullComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APlayerBase* Player = OwnerPlayer.Get();

    if (!IsValid(Player))
    {
        StopPushPull();
        return;
    }

    // 플레이어의 기존 이동 입력을 사용
    UpdatePushPull(Player->GetLastMovementInputVector(), DeltaTime);
}

void UPushPullComponent::UpdateWalkSpeedLimit(UCharacterMovementComponent* Movement, float MoveSpeed)
{
    if (!IsValid(Movement) || !bHasWalkSpeedOverride)
    {
        return;
    }

    // GAS 등 다른 기능이 걷기 속도를 변경했다면
    // 새 속도를 복구 기준으로 사용
    if (!FMath::IsNearlyEqual(Movement->MaxWalkSpeed, AppliedMaxWalkSpeed))
    {
        OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
    }

    AppliedMaxWalkSpeed = FMath::Min(OriginalMaxWalkSpeed, MoveSpeed);

    Movement->MaxWalkSpeed = AppliedMaxWalkSpeed;
}

bool UPushPullComponent::TryStepUp(UStaticMeshComponent* Mesh, double DirectionSign, double ForwardDistance)
{
    UWorld* World = GetWorld();
    APlayerBase* Player = OwnerPlayer.Get();
    APushPullGimmick* Target = CurrentTarget.Get();

    if (!World
        || !IsValid(Player)
        || !IsValid(Target)
        || !IsValid(Mesh)
        || !Mesh->IsSimulatingPhysics()
        || !FMath::IsFinite(ForwardDistance)
        || ForwardDistance <= 0.0
        || !FMath::IsFinite(DirectionSign)
        || FMath::IsNearlyZero(DirectionSign))
    {
        return false;
    }

    const FPushPullConfigRow& Config = Target->GetPushPullConfig();

    if (!FMath::IsFinite(Config.MaxStepHeight) || Config.MaxStepHeight <= 0.0f)
    {
        return false;
    }

    const double MaxStepHeight = static_cast<double>(Config.MaxStepHeight);

    const double MinFloorNormalZ = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(Config.MaxSlopeAngle)));

    constexpr double Clearance = 0.5;
    constexpr double GroundCheckDistance = 3.0;

    const FVector StartLocation = Mesh->GetComponentLocation();
    const FQuat Rotation = Mesh->GetComponentQuat();

    FComponentQueryParams QueryParams(SCENE_QUERY_STAT(PushPullStepUp), Target);

    QueryParams.AddIgnoredActor(Player);
    QueryParams.bIgnoreTouches = true;

    // 실제 접촉한 표면의 법선 사용
    const auto GetSurfaceNormal = [](const FHitResult& Hit)
        {
            FVector Normal = Hit.ImpactNormal.GetSafeNormal();

            if (Normal.IsNearlyZero())
            {
                Normal = Hit.Normal.GetSafeNormal();
            }

            return Normal;
        };

    // 이동을 막는 충돌 중 가장 가까운 결과 찾기
    const auto SweepFirstBlocking = [&](const FVector& Start, const FVector& End, FHitResult& OutHit) -> bool
        {
            TArray<FHitResult> Hits;

            World->ComponentSweepMulti(Hits, Mesh, Start, End, Rotation, QueryParams);

            const FVector Delta = End - Start;
            bool bFound = false;

            for (const FHitResult& Hit : Hits)
            {
                if (!Hit.bBlockingHit)
                {
                    continue;
                }

                const FVector Normal = GetSurfaceNormal(Hit);

                // 접촉면에서 멀어지거나 나란히 이동하는 경우 제외
                if (FVector::DotProduct(Delta, Normal)
                    >= -UE_KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                if (!bFound || Hit.Time < OutHit.Time)
                {
                    OutHit = Hit;
                    bFound = true;
                }
            }

            return bFound;
        };

    // 물체가 바닥 가까이에 있을 때만 턱 넘기 허용
    FHitResult GroundHit;

    if (!SweepFirstBlocking(StartLocation, StartLocation - FVector::UpVector * GroundCheckDistance, GroundHit))
    {
        return false;
    }

    if (GetSurfaceNormal(GroundHit).Z + KINDA_SMALL_NUMBER
        < MinFloorNormalZ)
    {
        return false;
    }

    // 깊이 겹쳐 있는 물체를 턱 넘기로 강제 이동시키지 않음
    if (GroundHit.bStartPenetrating && GroundHit.PenetrationDepth > 1.0f)
    {
        return false;
    }

    // 위쪽에 물체 전체가 들어갈 공간이 있는지 검사
    double UpDistance = MaxStepHeight + Clearance;
    FHitResult CeilingHit;

    if (SweepFirstBlocking(StartLocation, StartLocation + FVector::UpVector * UpDistance, CeilingHit))
    {
        if (CeilingHit.bStartPenetrating)
        {
            return false;
        }

        UpDistance = FMath::Max(0.0, UpDistance * static_cast<double>(CeilingHit.Time) - Clearance);
    }

    if (UpDistance <= Clearance)
    {
        return false;
    }

    const FVector RaisedLocation = StartLocation + FVector::UpVector * UpDistance;

    const FVector ForwardDelta( FMath::Sign(DirectionSign) * ForwardDistance, 0.0, 0.0);

    const FVector RaisedForwardLocation = RaisedLocation + ForwardDelta;

    // 높이를 올린 상태에서도 앞이 막혀 있으면 높은 벽
    FHitResult ForwardHit;

    if (SweepFirstBlocking(RaisedLocation, RaisedForwardLocation, ForwardHit))
    {
        return false;
    }

    // 앞으로 이동한 위치 아래에 착지할 바닥이 있는지 검사
    const FVector DownDelta = -FVector::UpVector * (UpDistance + GroundCheckDistance);

    FHitResult LandingHit;

    if (!SweepFirstBlocking(RaisedForwardLocation, RaisedForwardLocation + DownDelta, LandingHit))
    {
        return false;
    }

    if (LandingHit.bStartPenetrating || GetSurfaceNormal(LandingHit).Z + KINDA_SMALL_NUMBER < MinFloorNormalZ)
    {
        return false;
    }

    const FVector SurfaceLocation = RaisedForwardLocation + DownDelta * static_cast<double>(LandingHit.Time);

    const double StepHeight = SurfaceLocation.Z - StartLocation.Z;

    // 실제 턱이 있고, 허용 높이 이내인 경우만 이동
    if (StepHeight <= 0.1 || StepHeight > MaxStepHeight + 0.1)
    {
        return false;
    }

    const FVector LandingLocation = SurfaceLocation + FVector::UpVector * Clearance;

    FVector SavedVelocity = Mesh->GetPhysicsLinearVelocity();

    // 검사한 위·앞·아래 경로의 최종 위치로 보정
    Mesh->SetWorldLocation(LandingLocation, false, nullptr, ETeleportType::TeleportPhysics);

    // 위치 이동으로 발생한 이벤트가 조작을 종료할 수 있음
    if (IsValid(Mesh)
        && IsValid(Target)
        && CurrentTarget.Get() == Target
        && Target->GetControllingPlayer() == Player
        && Mesh->IsSimulatingPhysics())
    {
        // 턱 위에 착지한 순간의 수직 속도 정리
        SavedVelocity.Z = 0.0;

        Mesh->SetPhysicsLinearVelocity(SavedVelocity);
        Mesh->WakeAllRigidBodies();
    }

    return true;
}

bool UPushPullComponent::SweepBlockingMovement(UPrimitiveComponent* Component, const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
    OutHit = FHitResult();

    UWorld* World = GetWorld();

    if (!World || !IsValid(Component))
    {
        return false;
    }

    const FVector Delta = End - Start;

    if (Delta.IsNearlyZero())
    {
        return false;
    }

    FComponentQueryParams QueryParams(SCENE_QUERY_STAT(PushPullSharedMovement), Component->GetOwner());

    // 함께 움직이는 플레이어와 상자는 경로 검사에서 서로 제외
    QueryParams.AddIgnoredActor(GetOwner());

    if (CurrentTarget.IsValid())
    {
        QueryParams.AddIgnoredActor(CurrentTarget.Get());
    }

    QueryParams.bIgnoreTouches = true;

    TArray<FHitResult> Hits;

    World->ComponentSweepMulti(Hits, Component, Start, End, Component->GetComponentQuat(), QueryParams);

    bool bFoundBlockingHit = false;

    for (const FHitResult& Hit : Hits)
    {
        if (!Hit.bBlockingHit)
        {
            continue;
        }

        FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();

        if (SurfaceNormal.IsNearlyZero())
        {
            SurfaceNormal = Hit.Normal.GetSafeNormal();
        }

        // 바닥과 나란히 움직이거나 접촉면에서 멀어지는 경우 허용
        if (!SurfaceNormal.IsNearlyZero() && FVector::DotProduct(Delta, SurfaceNormal) >= -UE_KINDA_SMALL_NUMBER)
        {
            continue;
        }

        // 이동을 막는 충돌 중 가장 가까운 결과 선택
        if (!bFoundBlockingHit || Hit.Time < OutHit.Time)
        {
            OutHit = Hit;
            bFoundBlockingHit = true;
        }
    }

    return bFoundBlockingHit;
}

bool UPushPullComponent::FindSharedMoveLocation(UPrimitiveComponent* Component, double DeltaX, double MaxStepHeight, double MaxSlopeAngle, FVector& OutLocation, bool& bOutOnGround) const
{
    OutLocation = FVector::ZeroVector;
    bOutOnGround = false;

    if (!IsValid(Component) || !GetWorld())
    {
        return false;
    }

    const FVector Start = Component->GetComponentLocation();
    OutLocation = Start;

    if (!FMath::IsFinite(DeltaX)
        || !FMath::IsFinite(MaxStepHeight)
        || !FMath::IsFinite(MaxSlopeAngle)
        || MaxStepHeight < 0.0
        || MaxSlopeAngle < 0.0
        || MaxSlopeAngle > 80.0)
    {
        return false;
    }

    constexpr double GroundProbeDistance = 6.0;
    constexpr double SurfaceGap = 0.5;

    const double MinFloorNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxSlopeAngle));

    const auto GetSurfaceNormal = [](const FHitResult& Hit)
        {
            FVector Normal = Hit.ImpactNormal.GetSafeNormal();

            if (Normal.IsNearlyZero())
            {
                Normal = Hit.Normal.GetSafeNormal();
            }

            return Normal;
        };

    // 특정 위치 아래의 바닥을 확인하고 착지 위치 계산
    const auto FindLanding = [&](const FVector& From, double DownDistance, FVector& OutLanding, bool& bHasFloor) -> bool
        {
            OutLanding = From;
            bHasFloor = false;

            FHitResult FloorHit;

            if (!SweepBlockingMovement(Component, From, From - FVector::UpVector * DownDistance, FloorHit))
            {
                // 아래에 바닥이 없는 경우
                return true;
            }

            const FVector FloorNormal = GetSurfaceNormal(FloorHit);

            // 너무 가파른 면은 받쳐 주는 바닥으로 인정하지 않음
            if (FloorNormal.Z + KINDA_SMALL_NUMBER < MinFloorNormalZ)
            {
                return false;
            }

            // 깊이 겹친 상태를 강제 위치 이동으로 해결하지 않음
            if (FloorHit.bStartPenetrating && FloorHit.PenetrationDepth > 1.0f)
            {
                return false;
            }

            OutLanding = From - FVector::UpVector * DownDistance * static_cast<double>(FloorHit.Time);

            OutLanding.Z += SurfaceGap;

            if (FloorHit.bStartPenetrating)
            {
                OutLanding.Z += FloorHit.PenetrationDepth;
            }

            bHasFloor = true;
            return true;
        };

    // 현재 발밑에 가까운 바닥이 있는지 확인
    FHitResult CurrentFloorHit;

    const bool bFoundCurrentFloor = SweepBlockingMovement(
        Component,
        Start,
        Start - FVector::UpVector * GroundProbeDistance,
        CurrentFloorHit);

    if (bFoundCurrentFloor && CurrentFloorHit.bStartPenetrating && CurrentFloorHit.PenetrationDepth > 1.0f)
    {
        return false;
    }

    const bool bStartedOnGround = bFoundCurrentFloor && GetSurfaceNormal(CurrentFloorHit).Z + KINDA_SMALL_NUMBER >= MinFloorNormalZ;

    const FVector HorizontalDelta(DeltaX, 0.0, 0.0);
    const FVector FlatEnd = Start + HorizontalDelta;

    FHitResult ForwardHit;

    // 먼저 높이를 바꾸지 않고 이동할 수 있는지 검사
    if (!SweepBlockingMovement(Component, Start, FlatEnd, ForwardHit))
    {
        // 이미 공중에 있다면 수평 경로만 확인
        // 낙하 처리는 실제 이동 단계에서 담당
        if (!bStartedOnGround)
        {
            OutLocation = FlatEnd;
            return true;
        }

        // 평지와 내리막의 바닥 높이에 맞춘 도착 위치 계산
        return FindLanding(FlatEnd, MaxStepHeight + GroundProbeDistance, OutLocation, bOutOnGround);
    }

    // 공중에서는 벽을 타고 올라가거나 턱을 넘지 않음
    if (!bStartedOnGround)
    {
        return false;
    }

    const FVector ForwardNormal = GetSurfaceNormal(ForwardHit);

    // 허용된 경사면이라면 면을 따라 올라가는 경로 검사
    if (ForwardNormal.Z + KINDA_SMALL_NUMBER >= MinFloorNormalZ)
    {
        const double Rise = -DeltaX * ForwardNormal.X / ForwardNormal.Z;

        const FVector RampEnd = FlatEnd + FVector::UpVector * Rise;

        FHitResult RampHit;

        if (!SweepBlockingMovement(Component, Start, RampEnd, RampHit))
        {
            FVector Landing;
            bool bHasFloor = false;

            if (FindLanding(RampEnd, MaxStepHeight + GroundProbeDistance, Landing, bHasFloor) && bHasFloor)
            {
                OutLocation = Landing;
                bOutOnGround = true;
                return true;
            }
        }
    }

    // 경사 이동이 불가능하면 낮은 턱인지 검사
    if (MaxStepHeight <= 0.0)
    {
        return false;
    }

    double UpDistance = MaxStepHeight + SurfaceGap;
    FHitResult CeilingHit;

    // 위쪽 공간 검사
    if (SweepBlockingMovement(Component, Start, Start + FVector::UpVector * UpDistance, CeilingHit))
    {
        if (CeilingHit.bStartPenetrating)
        {
            return false;
        }

        UpDistance = FMath::Max(0.0, UpDistance * static_cast<double>(CeilingHit.Time) - SurfaceGap);
    }

    if (UpDistance <= UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector RaisedStart = Start + FVector::UpVector * UpDistance;

    const FVector RaisedEnd = RaisedStart + HorizontalDelta;

    FHitResult RaisedForwardHit;

    // 높이를 올린 상태에서 앞쪽 공간 검사
    if (SweepBlockingMovement(Component, RaisedStart, RaisedEnd, RaisedForwardHit))
    {
        return false;
    }

    FVector Landing;
    bool bHasFloor = false;

    // 앞으로 이동한 위치 아래의 착지 지점 검사
    if (!FindLanding(RaisedEnd, UpDistance + MaxStepHeight + GroundProbeDistance, Landing, bHasFloor) || !bHasFloor)
    {
        return false;
    }

    // 실제 올라가는 높이가 설정한 턱 높이를 넘으면 차단
    const double Rise = Landing.Z - Start.Z;

    if (Rise > MaxStepHeight + SurfaceGap + 0.1)
    {
        return false;
    }

    OutLocation = Landing;
    bOutOnGround = true;
    return true;
}

bool UPushPullComponent::BeginSharedMovement(UStaticMeshComponent* Mesh)
{
    if (bSharedMovementActive)
    {
        return false;
    }

    APlayerBase* Player = OwnerPlayer.Get();
    APushPullGimmick* Target = CurrentTarget.Get();

    if (!IsValid(Player)
        || !IsValid(Target)
        || !IsValid(Mesh)
        || Target->GetPhysicsMesh() != Mesh
        || Target->GetControllingPlayer() != Player
        || !Mesh->IsSimulatingPhysics())
    {
        return false;
    }

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    if (!IsValid(Movement) || !Movement->IsMovingOnGround() || !Movement->IsComponentTickEnabled())
    {
        return false;
    }

    FBodyInstance& Body = Mesh->BodyInstance;

    const bool bPreviousLockX = Body.bLockXTranslation;

    // 상자가 독립적인 물리 힘으로 좌우 이동하지 않도록 잠금
    // Z축 잠금 설정은 변경하지 않음
    Body.bLockXTranslation = true;
    Body.SetDOFLock(EDOFMode::SixDOF);

    if (Body.DOFConstraint == nullptr)
    {
        Body.bLockXTranslation = bPreviousLockX;
        Body.SetDOFLock(EDOFMode::SixDOF);
        return false;
    }

    // 남아 있는 수평 물리 속도만 제거
    FVector BoxVelocity = Mesh->GetPhysicsLinearVelocity();
    BoxVelocity.X = 0.0;
    BoxVelocity.Y = 0.0;

    Mesh->SetPhysicsLinearVelocity(BoxVelocity);

    bSavedMovementTickEnabled = Movement->IsComponentTickEnabled();

    // 이전 걷기 속도와 이동 입력이 남지 않도록 정리
    Movement->StopMovementImmediately();
    Player->ConsumeMovementInputVector();

    // 조작 중 이동은 공통 이동 함수가 담당
    Movement->SetComponentTickEnabled(false);

    bSharedMovementActive = true;
    return true;
}

void UPushPullComponent::EndSharedMovement()
{
    if (!bSharedMovementActive)
    {
        return;
    }

    // 종료 중 다른 이벤트가 발생해도 중복 복구하지 않도록 처리
    bSharedMovementActive = false;

    APlayerBase* Player = OwnerPlayer.Get();

    if (!IsValid(Player))
    {
        return;
    }

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    if (!IsValid(Movement))
    {
        return;
    }

    if (Movement->IsMovingOnGround() && !Player->IsActorBeingDestroyed())
    {
        // 공통 이동으로 위치가 달라졌으므로 현재 발밑 다시 확인
        FFindFloorResult FloorResult;

        Movement->FindFloor(Player->GetActorLocation(), FloorResult, false);

        Movement->CurrentFloor = FloorResult;

        if (FloorResult.IsWalkableFloor())
        {
            // 지상에서 손을 놓았을 때 남은 이동 속도 제거
            Movement->Velocity = FVector::ZeroVector;

            Player->SetBase(FloorResult.HitResult.Component.Get(), FloorResult.HitResult.BoneName);
        }
        else
        {
            // 발밑이 없다면 일반 낙하로 복귀
            Player->SetBase(nullptr);
            Movement->SetMovementMode(MOVE_Falling);
        }
    }

    Movement->bForceNextFloorCheck = true;
    Movement->UpdateComponentVelocity();

    // 전환 전의 자동 갱신 상태 복구
    Movement->SetComponentTickEnabled(bSavedMovementTickEnabled);
}

