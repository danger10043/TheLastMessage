// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/PushPullComponent.h"

#include "Character/Player/PlayerBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/CarryComponent.h"
#include "Gimmick/PushPull/PushPullGimmick.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

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

    if (!IsValid(Player) || Player->IsDead() || Player->IsHitReacting() || !IsValid(Target) || !IsValid(PhysicsHandle))
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
    const float DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), Anchor->GetComponentLocation()
    );

    if (DistanceSquared > FMath::Square(Config.MaxSeparationDistance))
    {
        return false;
    }

    ActivePlaneNormal = MovementPlaneNormal.GetSafeNormal();

    // 플레이어에게 이동 평면이 설정돼 있으면 그 평면을 사용
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

    TargetLocation = Mesh->GetComponentLocation();
    TargetRotation = Mesh->GetComponentRotation();

    // 현재 위치와 회전을 기준으로 물체 잡기
    PhysicsHandle->GrabComponentAtLocationWithRotation(Mesh, NAME_None, TargetLocation, TargetRotation
    );

    if (PhysicsHandle->GetGrabbedComponent() != Mesh)
    {
        return false;
    }

    Target->BeginPushPull(Player);

    // 물체 쪽에서도 조작 시작이 정상 처리됐는지 확인
    if (Target->GetControllingPlayer() != Player)
    {
        PhysicsHandle->ReleaseComponent();
        return false;
    }

    CurrentTarget = Target;

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
        || !IsValid(PhysicsHandle)
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
        || Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision
        || PhysicsHandle->GetGrabbedComponent() != Mesh)
    {
        StopPushPull();
        return;
    }

    const FPushPullConfigRow& Config = Target->GetPushPullConfig();

    const float DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), Anchor->GetComponentLocation());

    // 플레이어와 물체가 너무 멀어지면 조작 종료
    if (DistanceSquared > FMath::Square(Config.MaxSeparationDistance))
    {
        StopPushPull();
        return;
    }

    UpdateWalkSpeedLimit(Movement, Config.MoveSpeed);

    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f || MoveDirection.ContainsNaN())
    {
        return;
    }

    // 이동 평면을 벗어나는 입력 제거
    const FVector PlanarDirection = FVector::VectorPlaneProject(MoveDirection, ActivePlaneNormal).GetClampedToMaxSize(1.0f);

    // 플레이어의 현재 제한 속도를 넘지 않도록 처리
    const float EffectiveMoveSpeed = FMath::Max(0.0f, FMath::Min(Config.MoveSpeed, Movement->MaxWalkSpeed));

    UWorld* World = GetWorld();

    if (!World)
    {
        StopPushPull();
        return;
    }

    const FVector MeshLocation = Mesh->GetComponentLocation();

    // 입력에 따라 이동하려는 목표 위치 계산
    const FVector DesiredLocation = TargetLocation + PlanarDirection * EffectiveMoveSpeed * DeltaTime;

    // 물체의 현재 위치에서 목표 위치까지의 이동량
    const FVector DesiredDelta = (DesiredLocation - MeshLocation).GetClampedToMaxSize(Config.MaxSeparationDistance);

    const double DesiredDistance = DesiredDelta.Size();

    // 이동량이 없으면 현재 위치 유지
    TargetLocation = MeshLocation;

    if (!DesiredDelta.IsNearlyZero())
    {
        FComponentQueryParams QueryParams(SCENE_QUERY_STAT(PushPullMovement), Target);

        QueryParams.AddIgnoredActor(Player);
        QueryParams.bIgnoreTouches = true;

        TArray<FHitResult> SweepHits;

        // 실제 메시의 충돌 형상으로 이동 경로 검사
        World->ComponentSweepMulti(
            SweepHits,
            Mesh,
            MeshLocation,
            MeshLocation + DesiredDelta,
            Mesh->GetComponentQuat(),
            QueryParams);

        double AllowedDistance = DesiredDistance;

        // 벽 표면에 목표점이 들어가지 않도록 약간의 여유 확보
        constexpr double CollisionMargin = 1.0;

        for (const FHitResult& Hit : SweepHits)
        {
            if (!Hit.bBlockingHit)
            {
                continue;
            }

            // 접촉면에서 멀어지거나 나란히 움직이는 경우는 허용
            const double IntoSurface = FVector::DotProduct(DesiredDelta, Hit.Normal);

            if (IntoSurface >= -UE_KINDA_SMALL_NUMBER)
            {
                continue;
            }

            const double HitDistance = DesiredDistance * FMath::Clamp(static_cast<double>(Hit.Time), 0.0, 1.0);

            const double SafeDistance = FMath::Max(0.0, HitDistance - CollisionMargin);

            AllowedDistance = FMath::Min(AllowedDistance, SafeDistance);
        }

        // 벽에 막히면 목표점도 벽 앞에서 멈춤
        TargetLocation = MeshLocation + DesiredDelta.GetSafeNormal() * AllowedDistance;
    }

    PhysicsHandle->SetTargetLocationAndRotation(TargetLocation, TargetRotation);
}

void UPushPullComponent::StopPushPull()
{
    APushPullGimmick* Target = CurrentTarget.Get();

    CurrentTarget.Reset();
    SetComponentTickEnabled(false);

    // 물체 잡기 해제
    if (IsValid(PhysicsHandle))
    {
        PhysicsHandle->ReleaseComponent();
    }

    // 물체에 기록된 조작자 정보 정리
    if (IsValid(Target) && Target->GetControllingPlayer() == OwnerPlayer.Get())
    {
        Target->EndPushPull();
    }

    // 컴포넌트가 적용한 속도 제한 복구
    if (bHasWalkSpeedOverride)
    {
        if (APlayerBase* Player = OwnerPlayer.Get())
        {
            if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
            {
                // 다른 기능이 이미 속도를 변경했다면 덮어쓰지 않음
                if (FMath::IsNearlyEqual(Movement->MaxWalkSpeed, AppliedMaxWalkSpeed))
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

    // 플레이어가 소유하는 전용 물리 핸들 생성
    PhysicsHandle = NewObject<UPhysicsHandleComponent>(Player);

    Player->AddInstanceComponent(PhysicsHandle);
    PhysicsHandle->RegisterComponent();

    PhysicsHandle->SetInterpolationSpeed(15.0f);
    PhysicsHandle->SetLinearStiffness(10000.0f);
    PhysicsHandle->SetLinearDamping(2000.0f);
    PhysicsHandle->SetAngularStiffness(10000.0f);
    PhysicsHandle->SetAngularDamping(2000.0f);

    // 캐릭터 이동이 입력을 처리한 뒤 마지막 이동 입력을 읽음
    if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
    {
        AddTickPrerequisiteComponent(Movement);
    }

    // 목표 위치를 갱신한 뒤 물리 핸들이 처리되도록 순서 지정
    PhysicsHandle->AddTickPrerequisiteComponent(this);
	
}

void UPushPullComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopPushPull();

    if (IsValid(PhysicsHandle))
    {
        PhysicsHandle->DestroyComponent();
        PhysicsHandle = nullptr;
    }

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

