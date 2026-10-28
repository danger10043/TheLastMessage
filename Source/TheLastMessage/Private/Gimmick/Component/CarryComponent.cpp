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
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Gimmick/Carry/CarryableGimmick.h"

UCarryComponent::UCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UCarryComponent::TryPickUp(ACarryableGimmick* Target)
{
    CacheOwner();
    APlayerBase* Player = OwnerPlayer.Get();
    if (!IsValid(Player) || !Player->HasAuthority() || Player->IsDead() || Player->IsHitReacting()
        || Player->IsCarryTransitionActive() || HasCarriedObject() || PendingPickup.IsValid()
        || bIsChangingCarryState || bIsClearingCarryState || !IsValid(Target)
        || !Player->GetCharacterMovement()->IsMovingOnGround() || !Target->ReserveForPickup(Player)) return false;
    const FVector Facing = FVector::VectorPlaneProject(
        Player->GetCharacterMovement()->ConstrainDirectionToPlane(Target->GetActorLocation() - Player->GetActorLocation()),
        Player->GetActorUpVector()).GetSafeNormal();
    if (!Facing.IsNearlyZero()) Player->SetActorRotation(FRotationMatrix::MakeFromXZ(Facing, Player->GetActorUpVector()).Rotator());
    PendingPickup = Target;
    Target->OnEndPlay.AddUniqueDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);
    Player->PlayCarryTransition(true, Target->UsesWideCarryPose());
    if (!Player->IsCarryTransitionActive())
    {
        CancelPendingPickup();
        return false;
    }
    // The hands contact the grounded prop at the middle of the crouch, not on input.
    GetWorld()->GetTimerManager().SetTimer(CarryContactTimer, this, &UCarryComponent::CompletePendingPickup,
        Player->GetCarryTransitionDuration() * 0.5f, false);
    SetComponentTickEnabled(true);
    return true;
}

void UCarryComponent::CancelPendingPickup()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CarryContactTimer);
    if (ACarryableGimmick* Target = PendingPickup.Get())
    {
        Target->ReleasePickupReservation(OwnerPlayer.Get());
        Target->OnEndPlay.RemoveDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);
    }
    PendingPickup.Reset();
}

void UCarryComponent::CompletePendingPickup()
{
    APlayerBase* Player = OwnerPlayer.Get();
    ACarryableGimmick* Target = PendingPickup.Get();
    const bool bCanCommit = IsValid(Player) && !Player->IsDead() && !Player->IsHitReacting()
        && Player->IsCarryTransitionActive() && Player->GetCharacterMovement()->IsMovingOnGround() && IsValid(Target);
    PendingPickup.Reset();
    if (IsValid(Target)) Target->OnEndPlay.RemoveDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);
    const bool bPickedUp = bCanCommit && CommitPickUp(Target);
    if (IsValid(Target)) Target->ReleasePickupReservation(Player);
    if (!bPickedUp)
    {
        if (IsValid(Player)) Player->CancelCarryTransition();
        SetComponentTickEnabled(false);
        return;
    }
    MotionStartRelativeTransform = Target->GetActorTransform().GetRelativeTransform(
        Player->GetMesh()->GetSocketTransform(CarrySocketName));
    MotionEndRelativeTransform = Target->GetCarryConfig().CarryOffset;
    MotionElapsed = 0.0f;
    MotionDuration = Player->GetCarryTransitionDuration() * 0.5f;
    CarryMotion = ECarryMotion::Lift;
}

bool UCarryComponent::FindGroundDropTransform(ACarryableGimmick* Target, FTransform& OutTransform) const
{
    APlayerBase* Player = OwnerPlayer.Get();
    UStaticMeshComponent* Mesh = Target ? Target->GetObjectMesh() : nullptr;
    if (!Player || !Mesh || !Mesh->GetStaticMesh() || !GetWorld()) return false;
    const FVector Up = Player->GetActorUpVector();
    const FVector Probe = Target->ConstrainDropLocation(Player->GetActorLocation()
        + Player->GetActorForwardVector() * Target->GetCarryConfig().DropDistance);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CarryGroundPlacement), false, Player);
    Params.AddIgnoredActor(Target);
    FHitResult Floor;
    if (!GetWorld()->LineTraceSingleByChannel(Floor, Probe + Up * 100.0f, Probe - Up * 400.0f, ECC_Visibility, Params)
        || FVector::DotProduct(Floor.ImpactNormal, Up) < 0.5f) return false;
    const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
    const FTransform MeshTransform = Mesh->GetComponentTransform();
    const FVector Extent = Bounds.BoxExtent * MeshTransform.GetScale3D().GetAbs();
    const float Support = FMath::Abs(FVector::DotProduct(MeshTransform.GetUnitAxis(EAxis::X), Up)) * Extent.X
        + FMath::Abs(FVector::DotProduct(MeshTransform.GetUnitAxis(EAxis::Y), Up)) * Extent.Y
        + FMath::Abs(FVector::DotProduct(MeshTransform.GetUnitAxis(EAxis::Z), Up)) * Extent.Z;
    const float Bottom = FVector::DotProduct(MeshTransform.TransformVector(Bounds.Origin), Up) - Support;
    OutTransform = Target->GetActorTransform();
    OutTransform.SetLocation(Floor.ImpactPoint + Up * (1.0f - Bottom));
    // Refuse a placement through a wall; keep the held object and let the player try elsewhere.
    const FVector CenterOffset = MeshTransform.TransformVector(Bounds.Origin);
    FHitResult Obstacle;
    if (GetWorld()->SweepSingleByChannel(Obstacle, Target->GetActorLocation() + CenterOffset,
        OutTransform.GetLocation() + CenterOffset, MeshTransform.GetRotation(), ECC_Visibility,
        FCollisionShape::MakeBox(Extent * 0.95f), Params)) return false;
    return true;
}

void UCarryComponent::CompleteDrop()
{
    ACarryableGimmick* Target = CarriedObject.Get();
    if (!IsValid(Target)) { ClearCarryState(); return; }
    Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    RestoreObjectState(Target, false);
    Target->OnDropped();
    FinishCarryState();
}

void UCarryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    APlayerBase* Player = OwnerPlayer.Get();
    if (!IsValid(Player) || Player->IsDead()) { ClearCarryState(); return; }
    if (PendingPickup.IsValid())
    {
        if (!Player->IsCarryTransitionActive() || Player->IsHitReacting()
            || !Player->GetCharacterMovement()->IsMovingOnGround())
        {
            CancelPendingPickup();
            Player->CancelCarryTransition();
            SetComponentTickEnabled(false);
        }
        return;
    }
    ACarryableGimmick* Target = CarriedObject.Get();
    if (CarryMotion == ECarryMotion::None || !IsValid(Target))
    {
        SetComponentTickEnabled(false);
        return;
    }
    if (!Player->IsCarryTransitionActive() || Player->IsHitReacting())
    {
        // Interrupted before release: retain ownership instead of losing the item mid-air.
        Target->GetRootComponent()->SetRelativeLocationAndRotation(Target->GetCarryConfig().CarryOffset.GetLocation(),
            Target->GetCarryConfig().CarryOffset.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
        CarryMotion = ECarryMotion::None;
        SetComponentTickEnabled(false);
        return;
    }
    MotionElapsed += DeltaTime;
    const float Alpha = FMath::Clamp(MotionElapsed / FMath::Max(0.01f, MotionDuration), 0.0f, 1.0f);
    const float Weight = FMath::SmoothStep(0.0f, 1.0f, Alpha);
    Target->GetRootComponent()->SetRelativeLocationAndRotation(
        FMath::Lerp(MotionStartRelativeTransform.GetLocation(), MotionEndRelativeTransform.GetLocation(), Weight),
        FQuat::Slerp(MotionStartRelativeTransform.GetRotation(), MotionEndRelativeTransform.GetRotation(), Weight),
        false, nullptr, ETeleportType::TeleportPhysics);
    if (Alpha >= 1.0f)
    {
        if (CarryMotion == ECarryMotion::Lower) CompleteDrop();
        else { CarryMotion = ECarryMotion::None; SetComponentTickEnabled(false); }
    }
}

bool UCarryComponent::CommitPickUp(ACarryableGimmick* Target)
{
    CacheOwner();

    APlayerBase* Player = OwnerPlayer.Get();
    UAbilitySystemComponent* ASC = OwnerASC.Get();

    if (bIsChangingCarryState
        || bIsClearingCarryState
        || HasCarriedObject()
        || ActiveCarryEffectHandle.IsValid()
        || bHasJumpVelocityOverride
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

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

    if (!IsValid(CarryParent) || !IsValid(ObjectMesh) || !IsValid(Movement) || !FMath::IsFinite(Movement->JumpZVelocity))
    {
        return false;
    }

    const FCarryConfigRow Config = Target->GetCarryConfig();

    // 잘못된 배율로 점프 속도가 변경되는 것을 방지
    if (!FMath::IsFinite(Config.JumpVelocityMultiplier) || Config.JumpVelocityMultiplier <= 0.0f || Config.JumpVelocityMultiplier > 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 점프 속도 배율은 0보다 크고 1 이하여야 합니다."), *Target->GetName());

        return false;
    }

    if (!CarrySocketName.IsNone() && !CarryParent->DoesSocketExist(CarrySocketName))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: 운반 소켓 %s를 찾지 못했습니다."), *GetName(), *CarrySocketName.ToString());

        return false;
    }

    TGuardValue<bool> ChangingGuard(bIsChangingCarryState, true);

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

    if (!NewEffectHandle.IsValid() || !IsValid(Player) || Player->IsDead()
        || !IsValid(Target) || !Target->CanBePickedUp(Player))
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

    // Contact happens at the bottom of the crouch. Keep the prop on the floor here;
    // the component raises it with the hands during the second half of the montage.
    Target->SetActorTransform(OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);

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

    // 집기에 성공한 뒤 점프 초기 속도 변경
    if (!FMath::IsNearlyEqual(Config.JumpVelocityMultiplier, 1.0f))
    {
        OriginalJumpZVelocity = Movement->JumpZVelocity;
        bHasJumpVelocityOverride = true;

        Movement->JumpZVelocity = OriginalJumpZVelocity * Config.JumpVelocityMultiplier;
    }

    // 운반 물체가 파괴되거나 레벨에서 제거될 때 정리
    Target->OnEndPlay.AddUniqueDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);

    OnCarriedObjectChanged.Broadcast(Target);

    return CarriedObject.Get() == Target;
}

bool UCarryComponent::DropCarriedObject()
{
    APlayerBase* Player = OwnerPlayer.Get();
    ACarryableGimmick* Target = CarriedObject.Get();
    if (bIsChangingCarryState || bIsClearingCarryState || !IsValid(Player) || Player->IsDead()
        || Player->IsHitReacting() || Player->IsCarryTransitionActive() || !Player->HasAuthority()
        || !IsValid(Target) || !Player->GetCharacterMovement()->IsMovingOnGround()) return false;
    FTransform Ground;
    if (!FindGroundDropTransform(Target, Ground)) return false;
    Player->PlayCarryTransition(false, Target->UsesWideCarryPose());
    if (!Player->IsCarryTransitionActive()) return false;
    MotionStartRelativeTransform = Target->GetActorTransform().GetRelativeTransform(
        Player->GetMesh()->GetSocketTransform(CarrySocketName));
    MotionEndRelativeTransform = Ground.GetRelativeTransform(Player->GetMesh()->GetSocketTransform(CarrySocketName));
    MotionElapsed = 0.0f;
    MotionDuration = Player->GetCarryTransitionDuration() * 0.5f;
    CarryMotion = ECarryMotion::Lower;
    SetComponentTickEnabled(true);
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
        || Player->IsCarryTransitionActive()
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
    Target->OnTransferred();

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
    CancelPendingPickup();
    CarryMotion = ECarryMotion::None;
    SetComponentTickEnabled(false);
    if (APlayerBase* Player = OwnerPlayer.Get()) Player->CancelCarryTransition();

    ACarryableGimmick* Target = CarriedObject.Get();

    if (IsValid(Target))
    {
        // 사망해도 아이템의 현재 월드 위치 유지
        Target->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform);

        // 물리 시뮬레이션 없이 기존 크기와 충돌 상태 복구
        RestoreObjectState(Target, false);

        // 아이템 자체의 수직 낙하 시작
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
    CarryMotion = ECarryMotion::None;
    SetComponentTickEnabled(false);
    const bool bHadCarryState = CarriedObject.IsValid() || ActiveCarryEffectHandle.IsValid() || bHasJumpVelocityOverride;

    if (ACarryableGimmick* Target = CarriedObject.Get())
    {
        Target->OnEndPlay.RemoveDynamic(this, &UCarryComponent::HandleCarriedObjectEndPlay);
    }

    CarriedObject.Reset();

    // 복구할 값을 보관하고 먼저 변경 상태 해제
    const bool bRestoreJumpVelocity = bHasJumpVelocityOverride;
    const float JumpVelocityToRestore = OriginalJumpZVelocity;

    bHasJumpVelocityOverride = false;
    OriginalJumpZVelocity = 0.0f;

    // 내려놓기, 사망, 물체 삭제 시 기존 점프 속도로 복구
    if (bRestoreJumpVelocity)
    {
        APlayerBase* Player = OwnerPlayer.Get();

        if (IsValid(Player))
        {
            UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

            if (IsValid(Movement))
            {
                Movement->JumpZVelocity = JumpVelocityToRestore;
            }
        }
    }

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
