// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmick/Component/CarryComponent.h"

#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerBase.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Gimmick/Carry/CarryableGimmick.h"

UCarryComponent::UCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UCarryComponent::TryPickUp(ACarryableGimmick* Target)
{
    CacheOwner();

    APlayerBase* Player = OwnerPlayer.Get();
    UAbilitySystemComponent* ASC = OwnerASC.Get();

    if (bIsChangingCarryState
        || bIsClearingCarryState
        || HasCarriedObject()
        || ActiveCarryEffectHandle.IsValid()
        || !IsValid(Player)
        || Player->IsDead()
        || !Player->HasAuthority()
        || !IsValid(ASC)
        || !ASC->AbilityActorInfo.IsValid()
        || !IsValid(Target)
        || !Target->CanBePickedUp(Player))
    {
        return false;
    }

    USkeletalMeshComponent* CarryParent = Player->GetMesh();
    UStaticMeshComponent* ObjectMesh = Target->GetObjectMesh();

    if (!IsValid(CarryParent) || !IsValid(ObjectMesh))
    {
        return false;
    }

    if (!CarrySocketName.IsNone() && !CarryParent->DoesSocketExist(CarrySocketName))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 운반 소켓 %s를 찾지 못했습니다."), *GetName(), *CarrySocketName.ToString());

        return false;
    }

    TGuardValue<bool> ChangingGuard(bIsChangingCarryState, true);

    const FCarryConfigRow Config = Target->GetCarryConfig();

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddSourceObject(Target);

    const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Config.CarryEffectClass, 1.0f, Context);

    if (!Spec.IsValid())
    {
        return false;
    }

    const FActiveGameplayEffectHandle NewEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    // 집기에 실패하면 이번에 적용한 효과를 제거
    const auto RemoveNewEffect = [ASC, NewEffectHandle]()
        {
            if (IsValid(ASC) && NewEffectHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(NewEffectHandle);
            }
        };

    if (!NewEffectHandle.IsValid() || !IsValid(Player) || Player->IsDead() || !IsValid(Target) || !Target->CanBePickedUp(Player))
    {
        RemoveNewEffect();
        return false;
    }

    const FTransform OriginalTransform = Target->GetActorTransform();

    USceneComponent* OriginalParent = ObjectMesh->GetAttachParent();

    const FName OriginalSocket = ObjectMesh->GetAttachSocketName();

    OriginalWorldScale = Target->GetActorScale3D();
    bWasSimulatingPhysics = ObjectMesh->IsSimulatingPhysics();
    bWasActorCollisionEnabled = Target->GetActorEnableCollision();

    ObjectMesh->SetSimulatePhysics(false);
    Target->SetActorEnableCollision(false);

    const bool bAttached = Target->AttachToComponent(CarryParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CarrySocketName);

    if (!bAttached)
    {
        RestoreObjectState(Target, true);
        RemoveNewEffect();
        return false;
    }

    Target->SetActorRelativeTransform(Config.CarryOffset, false, nullptr, ETeleportType::TeleportPhysics);

    // 플레이어 메시의 크기 때문에 물체 크기가 달라지는 것을 방지
    Target->SetActorScale3D(OriginalWorldScale * Config.CarryOffset.GetScale3D());

    Target->OnPickedUp(Player);

    if (Target->GetCarrier() != Player)
    {
        Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

        if (IsValid(OriginalParent))
        {
            Target->AttachToComponent(OriginalParent, FAttachmentTransformRules::KeepWorldTransform, OriginalSocket);
        }

        Target->SetActorTransform(OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);

        RestoreObjectState(Target, true);
        RemoveNewEffect();

        return false;
    }

    CarriedObject = Target;
    ActiveCarryEffectHandle = NewEffectHandle;

    // 운반 물체가 파괴되거나 레벨에서 제거될 때 정리
    Target->OnEndPlay.AddUniqueDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);

    OnCarriedObjectChanged.Broadcast(Target);

    return CarriedObject.Get() == Target;
}

bool UCarryComponent::DropCarriedObject()
{
    APlayerBase* Player = OwnerPlayer.Get();
    ACarryableGimmick* Target = CarriedObject.Get();

    if (bIsChangingCarryState
        || bIsClearingCarryState
        || !IsValid(Player)
        || Player->IsDead()
        || !Player->HasAuthority()
        || !IsValid(Target))
    {
        return false;
    }

    // 위치 검사가 성공하기 전에는 운반 상태를 바꾸지 않음
    FTransform DropTransform;

    if (!FindDropTransform(DropTransform))
    {
        return false;
    }

    TGuardValue<bool> ChangingGuard(bIsChangingCarryState, true);

    Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    Target->SetActorTransform(DropTransform, false, nullptr, ETeleportType::TeleportPhysics);

    RestoreObjectState(Target, true);
    Target->OnDropped();

    FinishCarryState();

    return true;
}

bool UCarryComponent::TransferCarriedObject(USceneComponent* TargetParent)
{
    APlayerBase* Player = OwnerPlayer.Get();
    ACarryableGimmick* Target = CarriedObject.Get();

    if (bIsChangingCarryState
        || bIsClearingCarryState
        || !IsValid(Player)
        || Player->IsDead()
        || !Player->HasAuthority()
        || !IsValid(Target)
        || !IsValid(TargetParent))
    {
        return false;
    }

    USceneComponent* ObjectRoot = Target->GetRootComponent();

    // 자기 자신이나 자신의 자식에게 부착하는 순환 관계 방지
    if (!IsValid(ObjectRoot) || TargetParent == ObjectRoot || TargetParent->IsAttachedTo(ObjectRoot))
    {
        return false;
    }

    TGuardValue<bool> ChangingGuard(bIsChangingCarryState, true);

    if (!Target->AttachToComponent(TargetParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        return false;
    }

    // 소켓에 전달한 물체는 부착 상태를 유지하도록 물리 시뮬레이션 중지
    RestoreObjectState(Target, false);
    Target->OnDropped();

    FinishCarryState();

    return true;
}

void UCarryComponent::ClearCarryState()
{
    if (bIsClearingCarryState)
    {
        return;
    }

    TGuardValue<bool> ClearingGuard(bIsClearingCarryState, true);

    ACarryableGimmick* Target = CarriedObject.Get();

    if (IsValid(Target))
    {
        FTransform DropTransform;
        const bool bFoundDropPosition = FindDropTransform(DropTransform);

        Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

        // 강제 정리에서도 안전한 위치를 찾았다면 그곳에 내려놓음
        // 찾지 못하면 현재 월드 위치에서 부착만 해제
        if (bFoundDropPosition)
        {
            Target->SetActorTransform(DropTransform, false, nullptr, ETeleportType::TeleportPhysics);
        }

        RestoreObjectState(Target, true);
        Target->OnDropped();
    }

    // 물체가 이미 사라졌어도 운반 효과는 반드시 정리
    FinishCarryState();
}

bool UCarryComponent::HasCarriedObject() const
{
    return CarriedObject.IsValid();
}

ACarryableGimmick* UCarryComponent::GetCarriedObject() const
{
    return CarriedObject.Get();
}

void UCarryComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheOwner();
}

void UCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearCarryState();

    OwnerPlayer.Reset();
    OwnerASC.Reset();

    Super::EndPlay(EndPlayReason);
}

void UCarryComponent::CacheOwner()
{
    APlayerBase* Player = Cast<APlayerBase>(GetOwner());

    OwnerPlayer = Player;
    OwnerASC = IsValid(Player) ? Player->GetAbilitySystemComponent() : nullptr;
}

bool UCarryComponent::FindDropTransform(FTransform& OutTransform) const
{
    APlayerBase* Player = OwnerPlayer.Get();
    ACarryableGimmick* Target = CarriedObject.Get();
    UWorld* World = GetWorld();

    if (!IsValid(Player) || !IsValid(Target) || !World)
    {
        return false;
    }

    UStaticMeshComponent* ObjectMesh = Target->GetObjectMesh();
    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
    UCapsuleComponent* Capsule = Player->GetCapsuleComponent();

    if (!IsValid(ObjectMesh) || !IsValid(Movement) || !IsValid(Capsule))
    {
        return false;
    }

    const float DropDistance = Target->GetCarryConfig().DropDistance;

    if (!FMath::IsFinite(DropDistance) || DropDistance <= 0.0f)
    {
        return false;
    }

    FVector GravityDirection = Movement->GetGravityDirection().GetSafeNormal();

    if (GravityDirection.IsNearlyZero())
    {
        GravityDirection = -Player->GetActorUpVector();
    }

    const FVector UpDirection = -GravityDirection;

    const FVector ForwardDirection = FVector::VectorPlaneProject(Player->GetActorForwardVector(), UpDirection).GetSafeNormal();

    if (ForwardDirection.IsNearlyZero())
    {
        return false;
    }

    FVector LocalMin;
    FVector LocalMax;
    ObjectMesh->GetLocalBounds(LocalMin, LocalMax);

    const FVector LocalCenter = (LocalMin + LocalMax) * 0.5;
    const FVector ScaledHalfSize = (LocalMax - LocalMin) * 0.5 * OriginalWorldScale.GetAbs();

    // 얇은 메시도 검사할 수 있도록 최소 두께 확보
    const FVector HalfSize(FMath::Max(ScaledHalfSize.X, 1.0), FMath::Max(ScaledHalfSize.Y, 1.0), FMath::Max(ScaledHalfSize.Z, 1.0));

    const FQuat DropRotation = Target->GetActorQuat().GetNormalized();

    const auto GetExtentAlong = [&HalfSize, &DropRotation](const FVector& Direction)
        {
            return
                FMath::Abs(FVector::DotProduct(Direction, DropRotation.GetAxisX())) * HalfSize.X
                + FMath::Abs(FVector::DotProduct(Direction, DropRotation.GetAxisY())) * HalfSize.Y
                + FMath::Abs(FVector::DotProduct(Direction, DropRotation.GetAxisZ())) * HalfSize.Z;
        };

    const double CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

    const FVector DesiredCenter = Player->GetActorLocation() + ForwardDirection * DropDistance;

    FCollisionQueryParams PathParams(SCENE_QUERY_STAT(CarryDropPath), false, Player);

    PathParams.AddIgnoredActor(Target);

    // 현재 중력 방향으로 바닥 찾기
    FHitResult FloorHit;

    const bool bFoundFloor = World->LineTraceSingleByChannel(
        FloorHit,
        DesiredCenter + UpDirection * CapsuleHalfHeight,
        DesiredCenter + GravityDirection * (CapsuleHalfHeight + DropDistance),
        ECC_WorldDynamic,
        PathParams);

    if (!bFoundFloor || FloorHit.bStartPenetrating)
    {
        return false;
    }

    const FVector FloorNormal = FloorHit.ImpactNormal.GetSafeNormal();

    // 옆으로 향한 벽을 바닥으로 취급하지 않음
    if (FVector::DotProduct(FloorNormal, UpDirection) < 0.5)
    {
        return false;
    }

    constexpr double SurfaceClearance = 2.0;

    const FVector BoxCenter = FloorHit.ImpactPoint + FloorNormal * (GetExtentAlong(FloorNormal) + SurfaceClearance);

    const FCollisionShape BoxShape = FCollisionShape::MakeBox(HalfSize);

    // 목적지가 플레이어 자신이나 다른 장애물과 겹치는지 확인
    FCollisionQueryParams SpaceParams(SCENE_QUERY_STAT(CarryDropSpace), false);

    SpaceParams.AddIgnoredActor(Target);

    if (World->OverlapBlockingTestByChannel(BoxCenter, DropRotation, ECC_WorldDynamic, BoxShape, SpaceParams))
    {
        return false;
    }

    const double StartHeightAdjustment = FMath::Max( 0.0, GetExtentAlong(UpDirection) - CapsuleHalfHeight + SurfaceClearance);

    const FVector PathStart = Player->GetActorLocation() + UpDirection * StartHeightAdjustment;

    // 목적지까지 물체 크기로 검사하여 벽 너머에 놓는 것을 방지
    FHitResult PathHit;

    if (World->SweepSingleByChannel(PathHit, PathStart, BoxCenter, DropRotation, ECC_WorldDynamic, BoxShape, PathParams))
    {
        return false;
    }

    // 메시 중심이 원점에서 벗어나 있어도 위치 보정
    const FVector CenterOffset = DropRotation.RotateVector(LocalCenter * OriginalWorldScale);

    OutTransform = FTransform(DropRotation, BoxCenter - CenterOffset, OriginalWorldScale);

    return true;
}

void UCarryComponent::RestoreObjectState(ACarryableGimmick* Target, bool bRestorePhysics)
{
    if (!IsValid(Target))
    {
        return;
    }

    Target->SetActorScale3D(OriginalWorldScale);
    Target->SetActorEnableCollision(bWasActorCollisionEnabled);

    UStaticMeshComponent* ObjectMesh = Target->GetObjectMesh();

    if (!IsValid(ObjectMesh))
    {
        return;
    }

    const bool bEnablePhysics = bRestorePhysics && bWasSimulatingPhysics;

    ObjectMesh->SetSimulatePhysics(bEnablePhysics);

    if (bEnablePhysics)
    {
        ObjectMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        ObjectMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
}

void UCarryComponent::FinishCarryState()
{
    const bool bHadCarryState = CarriedObject.IsValid() || ActiveCarryEffectHandle.IsValid();

    if (ACarryableGimmick* Target = CarriedObject.Get())
    {
        Target->OnEndPlay.RemoveDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);
    }

    CarriedObject.Reset();

    // 태그 변경 알림에서 정리가 다시 호출되어도 중복 제거하지 않도록 먼저 무효화
    const FActiveGameplayEffectHandle HandleToRemove = ActiveCarryEffectHandle;

    ActiveCarryEffectHandle.Invalidate();

    UAbilitySystemComponent* ASC = OwnerASC.Get();

    if (IsValid(ASC) && HandleToRemove.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(HandleToRemove);
    }

    if (bHadCarryState)
    {
        OnCarriedObjectChanged.Broadcast(nullptr);
    }
}

void UCarryComponent::HandleCarriedObjectEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
    ClearCarryState();
}
