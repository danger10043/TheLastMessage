// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/PushPullComponent.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ScopedMovementUpdate.h"
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
	// Apply the coordinated pair move after ground physics settles, then evaluate IK.
	// Otherwise the box's ground-clearance correction oscillates against gravity.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
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

    // 표면 기준 위치 정렬이 성공한 뒤 실제 유지 간격 저장
    InitialRelativeOffsetX = 0.0;

    // 이전 조작의 입력과 속도가 남지 않도록 초기화
    PendingMoveInput = 0.0f;
    SharedMoveSpeedX = 0.0;

    // 조작 중 빠르게 움직일 때 충돌 누락을 줄이도록 설정
    Mesh->SetUseCCD(true);

    // 조작 중에는 물체를 바라보는 방향 유지
    bOriginalOrientToMovement = Movement->bOrientRotationToMovement;
    Movement->bOrientRotationToMovement = false;

    // 조작 전 걷기 속도를 기록하고 제한 적용
    OriginalMaxWalkSpeed = Movement->MaxWalkSpeed;
    AppliedMaxWalkSpeed = OriginalMaxWalkSpeed;
    bHasWalkSpeedOverride = true;

    UpdateWalkSpeedLimit(Movement, Config.MoveSpeed);

    if (!BeginSharedMovement(Mesh))
    {
        StopPushPull();
        return false;
    }

    // 지정한 잡기 위치로 이동할 수 있어야 조작 시작
    if (!AlignPlayerToGrabDistance())
    {
        StopPushPull();
        return false;
    }

    const FBox Bounds = Mesh->Bounds.GetBox();
    const auto* PlayerMesh = Player->GetMesh();
    const FVector ShoulderL = PlayerMesh->GetSocketLocation(TEXT("upperarm_l"));
    const FVector ShoulderR = PlayerMesh->GetSocketLocation(TEXT("upperarm_r"));
    const double ChestZ = (ShoulderL.Z + ShoulderR.Z) * 0.5 - 12.0;
    GripOffsetZ = FMath::Clamp(ChestZ, Bounds.Min.Z + 6.0, Bounds.Max.Z - 6.0) - Bounds.GetCenter().Z;
    GripHalfWidth = FMath::Min(Bounds.GetExtent().Y * 0.6,
        FMath::Clamp(FVector::Dist(ShoulderL, ShoulderR) * 0.55, 10.0, 18.0));

    // 위치 정렬 중 발생한 겹침 이벤트로 조작이 끝날 수 있음
    if (!IsValid(Player)
        || Player->IsDead()
        || !IsValid(Target)
        || !bSharedMovementActive
        || CurrentTarget.Get() != Target
        || !Movement->IsMovingOnGround())
    {
        StopPushPull();
        return false;
    }

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
        || Player->IsAttacking()
        || Player->IsGrappling()
        || Player->IsCarryTransitionActive()
        || !IsValid(Target)
        || !Target->IsGimmickEnabled()
        || Target->GetControllingPlayer() != Player
        || !bSharedMovementActive)
    {
        StopPushPull();
        return;
    }

    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= SMALL_NUMBER || MoveDirection.ContainsNaN())
    {
        return;
    }

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    UCapsuleComponent* Capsule = Player->GetCapsuleComponent();

    UStaticMeshComponent* Mesh = Target->GetPhysicsMesh();

    USceneComponent* Anchor = Target->GetInteractionAnchor();

    if (!IsValid(Movement)
        || !IsValid(Capsule)
        || !IsValid(Mesh)
        || !IsValid(Anchor)
        || !Mesh->IsSimulatingPhysics()
        || Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision
        || Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        StopPushPull();
        return;
    }

    // 자동 갱신을 멈춘 동안에도 외부 힘과 발사 요청은 확인
    Movement->ApplyAccumulatedForces(DeltaTime);
    Movement->HandlePendingLaunch();

    // 이동 모드 변경 이벤트에서 조작이 종료될 수도 있음
    if (!bSharedMovementActive || CurrentTarget.Get() != Target)
    {
        if (bSharedMovementActive && !CurrentTarget.IsValid())
        {
            StopPushPull();
        }

        return;
    }

    if (!Movement->IsMovingOnGround())
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

    // 일반 이동 입력이 쌓였다가 조작 종료 후 적용되지 않도록 정리
    Player->ConsumeMovementInputVector();

    const FPushPullConfigRow Config = Target->GetPushPullConfig();

    if (!FMath::IsFinite(Config.MoveSpeed)
        || Config.MoveSpeed <= 0.0f
        || !FMath::IsFinite(Config.HorizontalAcceleration)
        || Config.HorizontalAcceleration <= 0.0f
        || !FMath::IsFinite(Config.MaxStepHeight)
        || Config.MaxStepHeight < 0.0f
        || !FMath::IsFinite(Config.MaxSlopeAngle)
        || Config.MaxSlopeAngle < 0.0f
        || Config.MaxSlopeAngle > 80.0f
        || !FMath::IsFinite(Config.MaxSeparationDistance)
        || Config.MaxSeparationDistance <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 밀기·당기기 데이터 테이블 설정을 확인해주세요."), *Target->GetName());

        StopPushPull();
        return;
    }

    if (FVector::DistSquared(Player->GetActorLocation(), Anchor->GetComponentLocation()) >
        FMath::Square(static_cast<double>(Config.MaxSeparationDistance)))
    {
        StopPushPull();
        return;
    }

    UpdateWalkSpeedLimit(Movement, Config.MoveSpeed);

    const FVector PlayerStart = Capsule->GetComponentLocation();
    if (bApproaching)
    {
        UpdateApproach(DeltaTime);
        return;
    }
    const FVector BoxStart = Mesh->GetComponentLocation();

    if (PlayerStart.ContainsNaN() || BoxStart.ContainsNaN() || !FMath::IsFinite(InitialRelativeOffsetX))
    {
        StopPushPull();
        return;
    }

    const double StepTime = static_cast<double>(DeltaTime);

    const double MoveSpeed = FMath::Max(0.0, static_cast<double>(FMath::Min(Config.MoveSpeed, Movement->MaxWalkSpeed)));

    const double InputX = FMath::Clamp(static_cast<double>(MoveDirection.X), -1.0, 1.0);

    const double DesiredSpeedX = InputX * MoveSpeed;

    const double SpeedChange = static_cast<double>(Config.HorizontalAcceleration) * StepTime;

    // 플레이어와 상자에 사용할 공통 속도 계산
    SharedMoveSpeedX += FMath::Clamp(DesiredSpeedX - SharedMoveSpeedX, -SpeedChange, SpeedChange);

    const double RequestedDeltaX = SharedMoveSpeedX * StepTime;

    struct FPairMovePlan
    {
        FVector PlayerLocation = FVector::ZeroVector;
        FVector BoxLocation = FVector::ZeroVector;
        bool bPlayerOnGround = false;
        bool bBoxOnGround = false;
    };

    // 플레이어의 이동량을 기준으로 두 도착 위치를 함께 계산
    const auto BuildPairPlan = [&](double PlayerDeltaX, FPairMovePlan& OutPlan) -> bool
        {
            // 처음 잡았을 때의 간격에 맞는 상자 이동량 계산
            const double BoxDeltaX = PlayerStart.X + PlayerDeltaX + InitialRelativeOffsetX - BoxStart.X;

            const double PlayerSlopeAngle = FMath::Clamp(static_cast<double>(Movement->GetWalkableFloorAngle()), 0.0, 80.0);

            if (!FindSharedMoveLocation(
                Capsule,
                PlayerDeltaX,
                static_cast<double>(Movement->MaxStepHeight),
                PlayerSlopeAngle,
                OutPlan.PlayerLocation,
                OutPlan.bPlayerOnGround))
            {
                return false;
            }

            return FindSharedMoveLocation(
                Mesh,
                BoxDeltaX,
                static_cast<double>(Config.MaxStepHeight),
                static_cast<double>(Config.MaxSlopeAngle),
                OutPlan.BoxLocation,
                OutPlan.bBoxOnGround);
        };

    FPairMovePlan BestPlan;

    // 현재 위치에서도 정상적으로 지형을 판단할 수 있어야 함
    if (!BuildPairPlan(0.0, BestPlan))
    {
        StopPushPull();
        return;
    }

    if (!BestPlan.bPlayerOnGround)
    {
        Movement->SetMovementMode(MOVE_Falling);
        StopPushPull();
        return;
    }

    if (FMath::Abs(RequestedDeltaX) > UE_KINDA_SMALL_NUMBER)
    {
        FPairMovePlan FullPlan;

        if (BuildPairPlan(RequestedDeltaX, FullPlan))
        {
            BestPlan = FullPlan;
        }
        else
        {
            // 어느 한쪽이 막히면 둘 다 가능한 이동량까지 줄임
            double Low = 0.0;
            double High = 1.0;

            for (int32 Index = 0; Index < 6; ++Index)
            {
                const double Fraction = (Low + High) * 0.5;

                FPairMovePlan TestPlan;

                if (BuildPairPlan(RequestedDeltaX * Fraction, TestPlan))
                {
                    Low = Fraction;
                    BestPlan = TestPlan;
                }
                else
                {
                    High = Fraction;
                }
            }
        }
    }

    FBodyInstance& Body = Mesh->BodyInstance;

    if (!Body.bLockXTranslation || Body.DOFConstraint == nullptr)
    {
        StopPushPull();
        return;
    }

    SharedMoveSpeedX = (BestPlan.PlayerLocation.X - PlayerStart.X) / StepTime;

    {
        // 두 위치를 적용한 뒤 겹침 이벤트를 처리
        FScopedMovementUpdate PlayerMoveScope(Capsule, EScopedUpdate::DeferredUpdates);

        FScopedMovementUpdate BoxMoveScope(Mesh, EScopedUpdate::DeferredUpdates);

        // X축 잠금의 기준점을 새 상자 위치로 이동
        // Z축은 잠겨 있지 않으므로 중력에 따른 이동 유지
        Body.DOFConstraint->SetRefPosition(EConstraintFrame::Frame2, BestPlan.BoxLocation);

        Mesh->SetWorldLocation(BestPlan.BoxLocation, false, nullptr, ETeleportType::TeleportPhysics);

        FVector BoxVelocity = Mesh->GetPhysicsLinearVelocity();
        BoxVelocity.X = 0.0;
        BoxVelocity.Y = 0.0;

        if (BestPlan.bBoxOnGround)
        {
            BoxVelocity.Z = 0.0;
        }

        Mesh->SetPhysicsLinearVelocity(BoxVelocity);
        Mesh->WakeAllRigidBodies();

        Capsule->SetWorldLocation(BestPlan.PlayerLocation, false, nullptr, ETeleportType::TeleportPhysics);

        // 애니메이션에서도 실제 이동 속도를 읽을 수 있도록 갱신
        Movement->Velocity = (BestPlan.PlayerLocation - PlayerStart) / StepTime;

        Movement->UpdateComponentVelocity();
    }

    // 겹침 이벤트로 피격되거나 조작이 끝났다면 추가 처리하지 않음
    if (!bSharedMovementActive || CurrentTarget.Get() != Target)
    {
        if (bSharedMovementActive && !CurrentTarget.IsValid())
        {
            StopPushPull();
        }

        return;
    }

    if (!IsValid(Player) || !IsValid(Target) || !IsValid(Movement))
    {
        StopPushPull();
        return;
    }

    if (!BestPlan.bPlayerOnGround)
    {
        // 빈 바닥 결과를 전달해서 받침 대상 해제
        Movement->SetBaseFromFloor(FFindFloorResult{});
        Movement->SetMovementMode(MOVE_Falling);

        StopPushPull();
        return;
    }

    // 캐릭터 무브먼트가 사용하는 바닥 정보도 현재 위치로 갱신
    FFindFloorResult FloorResult;

    Movement->FindFloor(Player->GetActorLocation(), FloorResult, false);

    Movement->CurrentFloor = FloorResult;

    if (!FloorResult.IsWalkableFloor())
    {
        // 빈 바닥 결과를 전달해서 받침 대상 해제
        Movement->SetBaseFromFloor(FFindFloorResult{});
        Movement->SetMovementMode(MOVE_Falling);

        StopPushPull();
        return;
    }

    // 확인된 바닥 정보를 이용해 받침 대상 설정
    Movement->SetBaseFromFloor(FloorResult);

    Movement->SaveBaseLocation();
    // +추가----
    // 입력이나 물리 속도 대신 충돌 처리 후 실제 상자 이동량을 사용한다.
    Target->UpdatePushPullMovementSound(
        static_cast<float>((Mesh->GetComponentLocation().X - BoxStart.X) / StepTime), BestPlan.bBoxOnGround);
    // +--------
}

void UPushPullComponent::StopPushPull()
{
    APushPullGimmick* Target = CurrentTarget.Get();

    CurrentTarget.Reset();

    bApproaching = false;
    ApproachElapsed = 0.0f;

    PendingMoveInput = 0.0f;
    InitialRelativeOffsetX = 0.0;
    SharedMoveSpeedX = 0.0;

    SetComponentTickEnabled(false);

    EndSharedMovement();

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

    // 플레이어 액터 갱신 이후 공통 이동 처리
    AddTickPrerequisiteActor(Player);
    if (Player->GetMesh()) Player->GetMesh()->AddTickPrerequisiteComponent(this);
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

    const FVector MoveDirection(PendingMoveInput, 0.0f, 0.0f);

    // 다음 프레임에 입력이 없으면 정지하도록 매번 초기화
    PendingMoveInput = 0.0f;

    UpdatePushPull(MoveDirection, DeltaTime);
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

            // 받쳐 줄 수 없는 면이면 바닥이 없는 것으로 처리.
    // 수평 경로가 통과 가능한 경우 이동 후 중력으로 낙하하도록 허용.
            if (FloorNormal.Z + KINDA_SMALL_NUMBER < MinFloorNormalZ)
            {
                return true;
         
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

            // 확인된 바닥 정보를 이용해 받침 대상 설정
            Movement->SetBaseFromFloor(FloorResult);
        }
        else
        {
            // 발밑이 없다면 일반 낙하로 복귀
            // 빈 바닥 결과를 전달해서 받침 대상 해제
            Movement->SetBaseFromFloor(FFindFloorResult{});
            Movement->SetMovementMode(MOVE_Falling);
        }
    }

    Movement->bForceNextFloorCheck = true;
    Movement->UpdateComponentVelocity();

    // 전환 전의 자동 갱신 상태 복구
    Movement->SetComponentTickEnabled(bSavedMovementTickEnabled);
}

bool UPushPullComponent::AlignPlayerToGrabDistance()
{
    APlayerBase* Player = OwnerPlayer.Get();
    APushPullGimmick* Target = CurrentTarget.Get();

    if (!IsValid(Player) || !IsValid(Target) || !bSharedMovementActive)
    {
        return false;
    }

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    UCapsuleComponent* Capsule = Player->GetCapsuleComponent();

    UStaticMeshComponent* Mesh = Target->GetPhysicsMesh();

    USceneComponent* Anchor = Target->GetInteractionAnchor();

    if (!IsValid(Movement) || !IsValid(Capsule) || !IsValid(Mesh) || !IsValid(Anchor))
    {
        return false;
    }

    const FPushPullConfigRow& Config = Target->GetPushPullConfig();

    if (!FMath::IsFinite(Config.GrabDistance) || Config.GrabDistance <= 0.0f || Config.GrabDistance >= Config.MaxSeparationDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: GrabDistance와 MaxSeparationDistance를 확인해주세요."), *Target->GetName());

        return false;
    }

    // 단순 콜리전에서 주어진 위치와 가장 가까운 표면점 조회
    const auto ReadSurfacePoint = [&](const FVector& Location, FVector& OutSurfacePoint)
        -> bool {
            if (Location.ContainsNaN())
            {
                return false;
            }

            const float Distance = Mesh->GetClosestPointOnCollision(Location, OutSurfacePoint);

            return FMath::IsFinite(Distance) && Distance > 0.0f && !OutSurfacePoint.ContainsNaN();
        };

    const FVector StartLocation = Capsule->GetComponentLocation();

    FVector SurfacePoint;

    if (!ReadSurfacePoint(StartLocation, SurfacePoint))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 충돌 표면을 계산하지 못했습니다. 단순 콜리전과 플레이어 위치를 확인해주세요."), *Target->GetName());

        return false;
    }

    // 원점 대신 가까운 표면이 있는 방향으로 좌우 판단
    const double ToSurfaceX = SurfacePoint.X - StartLocation.X;

    if (FMath::Abs(ToSurfaceX) <= UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const double SideSign = FMath::Sign(ToSurfaceX);
    const double MinGap = Capsule->GetScaledCapsuleRadius() + 3.0;
    double Reach = Config.GrabDistance;
    const auto* PlayerMesh = Player->GetMesh();
    if (PlayerMesh && PlayerMesh->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE)
    {
        auto ArmLength = [PlayerMesh](const TCHAR* Upper, const TCHAR* Lower, const TCHAR* Hand)
        {
            return FVector::Dist(PlayerMesh->GetSocketLocation(Upper), PlayerMesh->GetSocketLocation(Lower))
                + FVector::Dist(PlayerMesh->GetSocketLocation(Lower), PlayerMesh->GetSocketLocation(Hand));
        };
        Reach = FMath::Min(ArmLength(TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l")),
            ArmLength(TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"))) * 0.65;
    }
    // Keep the configured distance within capsule clearance and the robot's reach.
    const double DesiredGap = FMath::Clamp(static_cast<double>(Config.GrabDistance), MinGap, FMath::Max(MinGap, Reach));
    GripSide = -SideSign;

    constexpr double GapTolerance = 0.25;
    constexpr int32 MaxIterations = 12;

    FVector CandidateLocation = StartLocation;
    FVector AlignedLocation = StartLocation;
    bool bConverged = false;

    for (int32 Index = 0; Index < MaxIterations; ++Index)
    {
        if (!ReadSurfacePoint(CandidateLocation, SurfacePoint))
        {
            return false;
        }

        // 처음 선택한 물체 옆면을 계속 기준으로 사용
        if (SideSign * (SurfacePoint.X - CandidateLocation.X) <= UE_KINDA_SMALL_NUMBER)
        {
            return false;
        }

        const double DesiredPlayerX = SurfacePoint.X - SideSign * DesiredGap;

        bool bOnGround = false;

        // 실제 위치는 바꾸지 않고 바닥에 맞는 후보 위치 계산
        if (!FindSharedMoveLocation(
            Capsule,
            DesiredPlayerX - StartLocation.X,
            static_cast<double>(Movement->MaxStepHeight),
            FMath::Clamp(static_cast<double>(Movement->GetWalkableFloorAngle()), 0.0, 80.0),
            AlignedLocation,
            bOnGround)
            || !bOnGround)
        {
            return false;
        }

        FVector CheckedSurfacePoint;

        if (!ReadSurfacePoint(AlignedLocation, CheckedSurfacePoint))
        {
            return false;
        }

        const double ActualGap = SideSign * (CheckedSurfacePoint.X - AlignedLocation.X);

        if (ActualGap > 0.0 && FMath::Abs(ActualGap - DesiredGap) <= GapTolerance)
        {
            bConverged = true;
            break;
        }

        // 곡면과 높이 차이를 반영해 후보 위치에서 다시 계산
        CandidateLocation = AlignedLocation;
    }

    if (!bConverged)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 설정한 표면 간격으로 잡기 위치를 계산하지 못했습니다."), *Target->GetName());

        return false;
    }

    if (FVector::DistSquared(AlignedLocation, Anchor->GetComponentLocation()) > FMath::Square(static_cast<double>(Config.MaxSeparationDistance)))
    {
        return false;
    }

    // The collision-checked destination is approached over several frames.
    ApproachStart = StartLocation;
    ApproachTarget = AlignedLocation;
    ApproachElapsed = 0.0f;
    ApproachDuration = FMath::Clamp(static_cast<float>(FMath::Abs(AlignedLocation.X-StartLocation.X)/350.0),0.2f,0.5f);
    bApproaching = !AlignedLocation.Equals(StartLocation,0.25);

    // 가까운 물체 표면을 바라보도록 설정
    const FVector Facing = FVector::VectorPlaneProject(FVector(SideSign, 0.0, 0.0), Player->GetActorUpVector()).GetSafeNormal();

    if (!Facing.IsNearlyZero())
    {
        Player->SetActorRotation(FRotationMatrix::MakeFromXZ(Facing, Player->GetActorUpVector()).Rotator());
    }

    // 표면 기준 정렬 결과를 기존 공통 이동에 사용할 간격으로 저장
    InitialRelativeOffsetX = Mesh->GetComponentLocation().X - Capsule->GetComponentLocation().X;

    Movement->Velocity = FVector::ZeroVector;
    Movement->UpdateComponentVelocity();

    return true;
}

void UPushPullComponent::UpdateApproach(float DeltaTime)
{
    APlayerBase* Player = OwnerPlayer.Get();
    APushPullGimmick* Target = CurrentTarget.Get();
    if (!Player || !Target || !bSharedMovementActive || DeltaTime <= SMALL_NUMBER) { StopPushPull(); return; }
    auto* Movement = Player->GetCharacterMovement();
    auto* Capsule = Player->GetCapsuleComponent();
    const FVector Start = Capsule->GetComponentLocation();
    ApproachElapsed += DeltaTime;
    const float T = FMath::Clamp(ApproachElapsed/ApproachDuration,0.0f,1.0f);
    const double Ease = T*T*(3.0f-2.0f*T);
    const double DesiredX = FMath::Lerp(ApproachStart.X,ApproachTarget.X,Ease);
    FVector Next; bool bOnGround = false;
    if (!FindSharedMoveLocation(Capsule,DesiredX-Start.X,Movement->MaxStepHeight,
        Movement->GetWalkableFloorAngle(),Next,bOnGround) || !bOnGround)
    { StopPushPull(); return; }
    FHitResult Hit;
    Capsule->SetWorldLocation(Next,true,&Hit,ETeleportType::TeleportPhysics);
    if (!bSharedMovementActive || CurrentTarget.Get()!=Target) return;
    if (Hit.bBlockingHit || !Capsule->GetComponentLocation().Equals(Next,0.1)) { StopPushPull(); return; }
    Movement->Velocity = (Next-Start)/DeltaTime;
    Movement->UpdateComponentVelocity();
    if (T >= 1.0f)
    {
        bApproaching = false;
        InitialRelativeOffsetX = Target->GetPhysicsMesh()->GetComponentLocation().X - Next.X;
        SharedMoveSpeedX = 0.0;
        Movement->Velocity = FVector::ZeroVector;
        Movement->UpdateComponentVelocity();
        Movement->FindFloor(Next,Movement->CurrentFloor,false);
        Movement->SetBaseFromFloor(Movement->CurrentFloor);
        Movement->SaveBaseLocation();
    }
}

bool UPushPullComponent::GetHandContacts(FVector& Left, FVector& Right) const
{
    const APlayerBase* Player = OwnerPlayer.Get();
    const APushPullGimmick* Target = CurrentTarget.Get();
    if (!Player || !Target || !Target->GetPhysicsMesh() || bApproaching) return false;
    const FBox Bounds = Target->GetPhysicsMesh()->Bounds.GetBox();
    const FVector Center(Bounds.GetCenter().X + GripSide*(Bounds.GetExtent().X+1.0),
        Bounds.GetCenter().Y,Bounds.GetCenter().Z+GripOffsetZ);
    Left = Center - Player->GetActorRightVector()*GripHalfWidth;
    Right = Center + Player->GetActorRightVector()*GripHalfWidth;
    return true;
}

