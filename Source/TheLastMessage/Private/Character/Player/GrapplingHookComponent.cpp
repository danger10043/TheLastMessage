#include "Character/Player/GrapplingHookComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemComponent.h"
#include "NativeGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GAS/WeaponGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "CableComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "UObject/UObjectIterator.h"
#include "Gimmick/SideScrollingSoftPlatform.h"

namespace
{
bool IsAuthoredGrappleTerrain(const UMeshComponent* Mesh, const AActor* Actor)
{
	if (Mesh->GetCollisionProfileName() == TEXT("GrappleTerrain")
		|| Actor->ActorHasTag(TEXT("GrappleTerrain"))) return true;
	// Named wall proxies represent solid terrain even when a separate kit mesh
	// supplies the art. Editor labels are unavailable in cooked builds; the editor
	// repair script persists the profile/tag for those builds.
	if (Actor->GetName().StartsWith(TEXT("Wall_"))) return true;
#if WITH_EDITOR
	if (Actor->GetActorLabel().StartsWith(TEXT("Wall_"))) return true;
#endif
	return false;
}

bool IsGrappleMesh(const UPrimitiveComponent* Component)
{
	const UMeshComponent* Mesh = Cast<UMeshComponent>(Component);
	if (!Mesh) return false;
	const FName Profile = Mesh->GetCollisionProfileName();
	if (Profile == TEXT("InvisibleWall") || Profile == TEXT("InvisibleWallDynamic")) return false;
	const AActor* Actor = Mesh->GetOwner();
	if (!Actor) return false;
	// 외형과 분리된 바닥이나 문 위 구조물의 충돌 표면은 실제 지형으로 취급
	// One-way platform art can be a single-sided top surface. Its hidden collision
	// mesh supplies the underside, so allow that mesh as a grapple target too.
	const bool bTerrainSurface = IsAuthoredGrappleTerrain(Mesh, Actor) || Actor->IsA<ASideScrollingSoftPlatform>();
	if (!bTerrainSurface && (!Mesh->IsVisible() || Mesh->bHiddenInGame || Actor->IsHidden())) return false;
	// 캐릭터가 소유하거나 부착한 장비도 제외
	for (const AActor* Owner = Actor; Owner; Owner = Owner->GetOwner())
	{
		if (Owner->IsA<APawn>()) return false;
	}
	for (const AActor* Parent = Actor->GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
	{
		if (Parent->IsA<APawn>()) return false;
	}
	if (const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh)) return StaticMesh->GetStaticMesh() != nullptr;
	if (const USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh)) return SkeletalMesh->GetSkeletalMeshAsset() != nullptr;
	return false;
}

void PrepareGrappleMeshes(UWorld* World)
{
	// 장식 메시도 태그나 에디터 충돌 설정 없이 훅 전용 검사에 참여
	for (TObjectIterator<UMeshComponent> It; It; ++It)
	{
		UMeshComponent* Mesh = *It;
		if (Mesh->GetWorld() != World || !Mesh->IsRegistered() || !IsGrappleMesh(Mesh)) continue;
		if (Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			// 원래 없던 이동 충돌이나 무기 판정이 생기지 않도록 훅 채널만 활성화
			Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			Mesh->SetGenerateOverlapEvents(false);
			Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		else if (Mesh->GetCollisionResponseToChannel(ECC_GameTraceChannel4) != ECR_Block)
		{
			Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
		}
	}
}
}

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GrapplingHookUnlock, "Unlock.GrapplingHook");

UGrapplingHookComponent::UGrapplingHookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ThrowAsset(TEXT("/Game/Test_Jihong/Animations/Grappling/AM_Grapple_Throw.AM_Grapple_Throw"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> PullAsset(TEXT("/Game/Test_Jihong/Animations/Grappling/AM_Grapple_Pull.AM_Grapple_Pull"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> LandAsset(TEXT("/Game/Test_Jihong/Animations/Grappling/AM_Grapple_Land.AM_Grapple_Land"));
	ThrowMontage = ThrowAsset.Object;
	PullMontage = PullAsset.Object;
	LandMontage = LandAsset.Object;
}

void UGrapplingHookComponent::BeginPlay()
{
	Super::BeginPlay();
	TraceChannel = ECC_GameTraceChannel4;
	CreateCable();
	if (APlayerBase* Player = Cast<APlayerBase>(GetOwner()))
	{
		AddTickPrerequisiteActor(Player);
		Player->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
		Player->GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &UGrapplingHookComponent::OnCapsuleHit);
		Player->LandedDelegate.AddDynamic(this, &UGrapplingHookComponent::OnLanded);
	}
}

void UGrapplingHookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndPull();
	StopPresentation();
	if (Cable) Cable->DestroyComponent();
	if (CableAnchor) CableAnchor->DestroyComponent();
	if (APlayerBase* Player = Cast<APlayerBase>(GetOwner()))
	{
		Player->GetCapsuleComponent()->OnComponentHit.RemoveDynamic(this, &UGrapplingHookComponent::OnCapsuleHit);
		Player->LandedDelegate.RemoveDynamic(this, &UGrapplingHookComponent::OnLanded);
		Player->GetCharacterMovement()->RemoveTickPrerequisiteComponent(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UGrapplingHookComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGrapplingHookComponent, ProbeState);
	DOREPLIFETIME_CONDITION(UGrapplingHookComponent, CooldownState, COND_OwnerOnly);
}

float UGrapplingHookComponent::GetCooldownTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0;
	const AGameStateBase* State = World->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float UGrapplingHookComponent::GetCooldownRemaining() const
{
	return FMath::Max(0.0f, CooldownState.EndTime - GetCooldownTime());
}

float UGrapplingHookComponent::GetCooldownProgress() const
{
	return CooldownState.Duration > 0 ? 1.0f - FMath::Clamp(GetCooldownRemaining() / CooldownState.Duration, 0.0f, 1.0f) : 1.0f;
}

bool UGrapplingHookComponent::IsGrappleUnlocked() const
{
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	const UAbilitySystemComponent* ASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(TAG_GrapplingHookUnlock);
}

void UGrapplingHookComponent::StartCooldown(float Duration)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	CooldownState.Duration = FMath::IsFinite(Duration) ? FMath::Max(0.0f, Duration) : 0.0f;
	CooldownState.EndTime = GetCooldownTime() + CooldownState.Duration;
	GetOwner()->ForceNetUpdate();
}

void UGrapplingHookComponent::RequestGrapple(FVector WorldAimPoint)
{
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	// 그래플링 훅 코어의 GE가 부여한 사용 권한이 있어야 훅을 요청할 수 있다.
	if (!IsGrappleUnlocked() || GetCooldownRemaining() > 0 || HasGrappleAnchor()) return;
	bPullRequested = true;
	if (Player->HasAuthority())
	{
		ProbeForAnchor(WorldAimPoint);
	}
	else if (Player->IsLocallyControlled())
	{
		ServerRequestGrapple(WorldAimPoint);
	}
}

void UGrapplingHookComponent::ReleaseGrapple()
{
	bPullRequested = false;
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority())
	{
		ClearAnchor();
	}
	else if (const APawn* Pawn = Cast<APawn>(GetOwner()); Pawn && Pawn->IsLocallyControlled())
	{
		EndPull();
		MulticastFinishPresentation_Implementation(bCompletedPull);
		// Hide the local tether immediately; authoritative state follows through replication.
		ProbeState = FGrapplingProbeState();
		UpdateCable();
		ServerReleaseGrapple();
	}
}

void UGrapplingHookComponent::CancelGrapple()
{
	APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player || !bMovementOverrideActive) return;
	const FVector ReleaseVelocity = Player->GetCharacterMovement()->Velocity * ReleaseVelocityValue;
	bCompletedPull = false;
	bPullRequested = false;
	if (Player->HasAuthority())
	{
		ClearAnchor();
		Player->GetCharacterMovement()->Velocity = ReleaseVelocity;
		Player->GrantGrappleCancelJump();
	}
	else if (Player->IsLocallyControlled())
	{
		EndPull();
		Player->GetCharacterMovement()->Velocity = ReleaseVelocity;
		StopPresentation();
		ProbeState = FGrapplingProbeState();
		UpdateCable();
		Player->GrantGrappleCancelJump();
		ServerCancelGrapple();
	}
}

void UGrapplingHookComponent::ServerCancelGrapple_Implementation()
{
	CancelGrapple();
}

void UGrapplingHookComponent::ServerReleaseGrapple_Implementation()
{
	bPullRequested = false;
	// An owning client's arrival can precede the next server movement update slightly.
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	bCompletedPull = Player && HasGrappleAnchor() && FVector::DistSquared(Player->GetActorLocation(), GetPullDestination()) <= FMath::Square(25.0f);
	ClearAnchor();
}

void UGrapplingHookComponent::OnRep_ProbeState()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (HasGrappleAnchor() && Pawn && Pawn->IsLocallyControlled() && bPullRequested)
	{
		BeginPull();
	}
	else
	{
		EndPull();
	}
	UpdateCable();
}

void UGrapplingHookComponent::ServerRequestGrapple_Implementation(FVector_NetQuantize WorldAimPoint)
{
	// 서버에서도 같은 요청 함수를 거쳐 사용 권한을 확인한다.
	RequestGrapple(WorldAimPoint);
}

void UGrapplingHookComponent::ClearAnchor()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (HasGrappleAnchor() || bMovementOverrideActive) StartCooldown(CooldownDuration);
		EndPull();
		MulticastFinishPresentation(bCompletedPull);
		bCompletedPull = false;
		ProbeState = FGrapplingProbeState();
		UpdateCable();
		GetOwner()->ForceNetUpdate();
	}
}

void UGrapplingHookComponent::ProbeForAnchor(FVector WorldAimPoint)
{
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player || Player->IsDead() || Player->IsHitReacting() || WorldAimPoint.ContainsNaN()
		|| Player->GetCharacterMovement()->MovementMode == MOVE_None
		|| Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking)) return;
	ClearAnchor();
	const FVector Start = Player->GetActorLocation();
	FVector Direction = WorldAimPoint - Start;
	const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	if (Movement->bConstrainToPlane)
	{
		Direction = FVector::VectorPlaneProject(Direction, Movement->GetPlaneConstraintNormal());
	}
	if (Direction.ContainsNaN() || !Direction.Normalize() || !FMath::IsFinite(MaxGrappleDistance) || MaxGrappleDistance <= 0.0f) return;
	const FVector End = Start + Direction * MaxGrappleDistance;
	PrepareGrappleMeshes(GetWorld());
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrapplingProbe), true, Player);
	// Ignore anything attached to the character, including future weapon actors.
	TArray<AActor*> AttachedActors;
	Player->GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors);
	bool bHit = false;
	// 숨겨진 충돌 프록시와 적을 건너뛰고 실제 메시 검사
	while (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params))
	{
		if (IsGrappleMesh(Hit.GetComponent()))
		{
			bHit = true;
			break;
		}
		if (!Hit.GetComponent()) break;
		Params.AddIgnoredComponent(Hit.GetComponent());
	}
	ProbeState.TraceStart = Start;
	ProbeState.TraceEnd = bHit ? Hit.ImpactPoint : End;
	ProbeState.Result = bHit ? EGrapplingProbeResult::Blocked : EGrapplingProbeResult::Miss;
	if (bHit && !Hit.bStartPenetrating)
	{
		ProbeState.Result = EGrapplingProbeResult::Connected;
		ProbeState.AnchorPoint = Hit.ImpactPoint;
		ProbeState.SurfaceNormal = Hit.ImpactNormal;
		BeginPull();
	}
	else
	{
		bPullRequested = false;
		StartCooldown(FailedCooldownDuration);
	}
	UpdateCable();
	GetOwner()->ForceNetUpdate();
}

FVector UGrapplingHookComponent::GetPullDestination() const
{
	const APlayerBase* Player = CastChecked<APlayerBase>(GetOwner());
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	const FVector Normal = ProbeState.SurfaceNormal.GetSafeNormal();
	const float SupportRadius = Capsule->GetScaledCapsuleRadius()
		+ (Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius())
		* FMath::Abs(FVector::DotProduct(Normal, Capsule->GetUpVector()));
	FVector Destination = ProbeState.AnchorPoint + Normal * (SupportRadius + FMath::Max(1.0f, SurfaceClearance));
	const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	if (Movement->bConstrainToPlane)
	{
		Destination = FVector::PointPlaneProject(Destination, Player->GetActorLocation(), Movement->GetPlaneConstraintNormal());
	}
	return Destination;
}

void UGrapplingHookComponent::BeginPull()
{
	APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player || bMovementOverrideActive || Player->IsDead() || !HasGrappleAnchor()) return;
	Player->CaptureGrappleJumpState();
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	// On an owning client, replicated flying mode may arrive before the anchor.
	SavedMovementMode = !Player->HasAuthority() && Movement->MovementMode == MOVE_Flying ? MOVE_Falling : Movement->MovementMode.GetValue();
	SavedCustomMovementMode = Movement->CustomMovementMode;
	SavedMaxFlySpeed = Movement->MaxFlySpeed;
	bMovementOverrideActive = true;
	bCompletedPull = false;
	ThrowDelayRemaining = GetThrowDuration();
	PullElapsedTime = 0.0f;
	StalledTime = 0.0f;
	LastDistanceToTarget = FVector::Distance(Player->GetActorLocation(), GetPullDestination());
	const FVector FacingDirection = FVector::VectorPlaneProject(ProbeState.AnchorPoint - Player->GetActorLocation(), FVector::UpVector).GetSafeNormal();
	if (!FacingDirection.IsNearlyZero()) Player->SetActorRotation(FacingDirection.Rotation());
	Player->StopJumping();
	Player->StopMovementForAttack();
	Movement->StopMovementImmediately();
	Movement->MaxFlySpeed = FMath::Max(1.0f, PullSpeed);
	// Flying suspends gravity while keeping CharacterMovement's capsule sweeps and replication.
	Movement->SetMovementMode(MOVE_Flying);
	if (Player->HasAuthority() && !bAddedGrapplingTag)
	{
		Player->GetAbilitySystemComponent()->AddLooseGameplayTag(WeaponGameplayTags::State_Grappling, 1, EGameplayTagReplicationState::TagAndCountToAll);
		bAddedGrapplingTag = true;
	}
	if (Player->HasAuthority()) MulticastStartPresentation();
}

void UGrapplingHookComponent::EndPull()
{
	APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player) return;
	if (bAddedGrapplingTag)
	{
		Player->GetAbilitySystemComponent()->RemoveLooseGameplayTag(WeaponGameplayTags::State_Grappling, 1, EGameplayTagReplicationState::TagAndCountToAll);
		bAddedGrapplingTag = false;
	}
	if (!bMovementOverrideActive) return;
	bMovementOverrideActive = false;
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->MaxFlySpeed = SavedMaxFlySpeed;
	Movement->StopMovementImmediately();
	Player->ConsumeMovementInputVector();
	if (!Player->IsDead() && Movement->MovementMode == MOVE_Flying)
	{
		// A formerly grounded character may now be in mid-air: let falling find its floor.
		const EMovementMode RestoreMode = SavedMovementMode == MOVE_Walking || SavedMovementMode == MOVE_NavWalking
			? MOVE_Falling : SavedMovementMode;
		Movement->SetMovementMode(RestoreMode, SavedCustomMovementMode);
	}
	Player->RestoreGrappleJumpState();
}

void UGrapplingHookComponent::UpdatePull(float DeltaTime)
{
	if (!bMovementOverrideActive || DeltaTime <= 0.0f) return;
	APlayerBase* Player = CastChecked<APlayerBase>(GetOwner());
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	const FVector Delta = GetPullDestination() - Player->GetActorLocation();
	const float Distance = Delta.Size();
	if (Player->IsDead() || Player->IsHitReacting() || Movement->MovementMode != MOVE_Flying)
	{
		ReleaseGrapple();
		return;
	}
	if (ThrowDelayRemaining > 0.0f)
	{
		ThrowDelayRemaining = FMath::Max(0.0f, ThrowDelayRemaining - DeltaTime);
		Movement->StopMovementImmediately();
		if (ThrowDelayRemaining > 0.0f) return;
	}
	PullElapsedTime += DeltaTime;
	StalledTime = Distance >= LastDistanceToTarget - 0.5f ? StalledTime + DeltaTime : 0.0f;
	LastDistanceToTarget = Distance;
	if (Player->IsDead() || Player->IsHitReacting() || Movement->MovementMode != MOVE_Flying
		|| Distance <= 5.0f || PullElapsedTime >= MaxPullDuration || StalledTime >= 0.25f
		|| Delta.ContainsNaN() || !FMath::IsFinite(PullSpeed) || PullSpeed <= 0.0f
		|| !FMath::IsFinite(MaxPullDuration) || MaxPullDuration <= 0.0f)
	{
		bCompletedPull = Distance <= 5.0f && !Player->IsDead() && !Player->IsHitReacting();
		ReleaseGrapple();
		return;
	}
	Player->ConsumeMovementInputVector();
	Movement->Velocity = Delta.GetSafeNormal() * FMath::Min(PullSpeed, Distance / DeltaTime);
}

void UGrapplingHookComponent::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bMovementOverrideActive && Hit.bBlockingHit) ReleaseGrapple();
}

void UGrapplingHookComponent::CreateCable()
{
	APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player || Cable || GetNetMode() == NM_DedicatedServer) return;
	CableAnchor = NewObject<USceneComponent>(Player, TEXT("GrappleCableAnchor"));
	Player->AddInstanceComponent(CableAnchor);
	CableAnchor->SetupAttachment(Player->GetRootComponent());
	CableAnchor->SetAbsolute(true, false, false);
	CableAnchor->RegisterComponent();
	Cable = NewObject<UCableComponent>(Player, TEXT("GrappleCable"));
	Player->AddInstanceComponent(Cable);
	Cable->SetupAttachment(Player->GetRootComponent());
	Cable->bAttachStart = true;
	Cable->bAttachEnd = true;
	Cable->SetAttachEndToComponent(CableAnchor);
	Cable->EndLocation = FVector::ZeroVector;
	Cable->CableWidth = FMath::Max(0.1f, CableWidth);
	Cable->NumSegments = 16;
	Cable->SolverIterations = 16;
	Cable->CableGravityScale = 0.25f;
	Cable->bEnableCollision = false;
	Cable->bResetAfterTeleport = true;
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cable->SetGenerateOverlapEvents(false);
	Cable->PrimaryComponentTick.TickGroup = TG_PostPhysics;
	Cable->AddTickPrerequisiteComponent(Player->GetMesh());
	if (CableMaterial) Cable->SetMaterial(0, CableMaterial);
	Cable->SetHiddenInGame(true);
	Cable->SetVisibility(false);
	Cable->RegisterComponent();
	Cable->SetComponentTickEnabled(false);
}

void UGrapplingHookComponent::UpdateCable()
{
	if (!Cable || !CableAnchor) return;
	if (!HasGrappleAnchor())
	{
		Cable->SetVisibility(false);
		Cable->SetHiddenInGame(true);
		Cable->SetComponentTickEnabled(false);
		return;
	}
	const APlayerBase* Player = CastChecked<APlayerBase>(GetOwner());
	USceneComponent* StartComponent = Player->GetRootComponent();
	FName StartSocket = NAME_None;
	if (Player->GetMesh()->DoesSocketExist(CableStartSocket))
	{
		StartComponent = Player->GetMesh();
		StartSocket = CableStartSocket;
	}
	if (Cable->GetAttachParent() != StartComponent || Cable->GetAttachSocketName() != StartSocket)
	{
		Cable->AttachToComponent(StartComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, StartSocket);
	}
	// An absolute scene anchor keeps the cable endpoint fixed even after movement and bone updates.
	CableAnchor->SetWorldLocation(ProbeState.AnchorPoint);
	Cable->CableLength = FVector::Distance(Cable->GetComponentLocation(), ProbeState.AnchorPoint) * 1.01f;
	if (!FMath::IsNearlyEqual(Cable->CableWidth, CableWidth))
	{
		Cable->CableWidth = FMath::Max(0.1f, CableWidth);
		Cable->MarkRenderStateDirty();
	}
	if (CableMaterial && Cable->GetMaterial(0) != CableMaterial) Cable->SetMaterial(0, CableMaterial);
	if (!Cable->IsVisible())
	{
		// Reset particles along the new tether instead of simulating from a previous shot.
		Cable->ReregisterComponent();
		Cable->SetVisibility(true);
		Cable->SetHiddenInGame(false);
		Cable->SetComponentTickEnabled(true);
	}
}

float UGrapplingHookComponent::GetThrowDuration() const
{
	return ThrowMontage && ThrowMontage->RateScale > UE_SMALL_NUMBER
		? FMath::Min(ThrowMontage->GetPlayLength() / ThrowMontage->RateScale, FMath::Max(0.0f, LaunchDelay)) : 0.0f;
}

bool UGrapplingHookComponent::CanPlayPresentation() const
{
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	return Player && !Player->IsDead() && !Player->IsHitReacting()
		&& !Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(WeaponGameplayTags::State_Attacking);
}

bool UGrapplingHookComponent::PlayGrappleMontage(UAnimMontage* Montage)
{
	APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	UAnimInstance* Anim = Player ? Player->GetMesh()->GetAnimInstance() : nullptr;
	return Montage && Anim && CanPlayPresentation() && Anim->Montage_Play(Montage) > 0.0f;
}

void UGrapplingHookComponent::StopPresentation()
{
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	UAnimInstance* Anim = Player ? Player->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim)
	{
		// Never stop a hit/death/attack montage that has replaced our slot animation.
		for (UAnimMontage* Montage : { ThrowMontage.Get(), PullMontage.Get(), LandMontage.Get() })
		{
			if (Montage && Anim->Montage_IsActive(Montage)) Anim->Montage_Stop(0.1f, Montage);
		}
	}
	bPresentationActive = false;
	AnimationPhase = EGrappleAnimationPhase::None;
	PresentationThrowRemaining = 0.0f;
	LandingWaitRemaining = 0.0f;
}

void UGrapplingHookComponent::MulticastStartPresentation_Implementation()
{
	StopPresentation();
	if (!CanPlayPresentation()) return;
	bPresentationActive = true;
	PresentationThrowRemaining = GetThrowDuration();
	AnimationPhase = PresentationThrowRemaining > 0.0f ? EGrappleAnimationPhase::Throw : EGrappleAnimationPhase::Pull;
	PlayGrappleMontage(AnimationPhase == EGrappleAnimationPhase::Throw ? ThrowMontage : PullMontage);
}

void UGrapplingHookComponent::MulticastFinishPresentation_Implementation(bool bCompleted)
{
	StopPresentation();
	if (!bCompleted || !CanPlayPresentation()) return;
	LandingWaitRemaining = FMath::Max(0.0f, LandingDetectionWindow);
	const APlayerBase* Player = CastChecked<APlayerBase>(GetOwner());
	if (Player->GetCharacterMovement()->IsMovingOnGround())
	{
		OnLanded(FHitResult());
	}
}

void UGrapplingHookComponent::OnLanded(const FHitResult& Hit)
{
	if (LandingWaitRemaining <= 0.0f) return;
	LandingWaitRemaining = 0.0f;
	if (PlayGrappleMontage(LandMontage)) AnimationPhase = EGrappleAnimationPhase::Land;
}

void UGrapplingHookComponent::UpdatePresentation(float DeltaTime)
{
	if (!CanPlayPresentation())
	{
		if (bPresentationActive || LandingWaitRemaining > 0.0f || AnimationPhase != EGrappleAnimationPhase::None) StopPresentation();
		return;
	}
	LandingWaitRemaining = FMath::Max(0.0f, LandingWaitRemaining - DeltaTime);
	if (bPresentationActive && AnimationPhase == EGrappleAnimationPhase::Throw)
	{
		PresentationThrowRemaining = FMath::Max(0.0f, PresentationThrowRemaining - DeltaTime);
		if (PresentationThrowRemaining <= 0.0f)
		{
			AnimationPhase = EGrappleAnimationPhase::Pull;
			PlayGrappleMontage(PullMontage);
		}
	}
	if (AnimationPhase == EGrappleAnimationPhase::Land)
	{
		const APlayerBase* Player = CastChecked<APlayerBase>(GetOwner());
		const UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
		if (!Anim || !LandMontage || !Anim->Montage_IsActive(LandMontage)) AnimationPhase = EGrappleAnimationPhase::None;
	}
}

void UGrapplingHookComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePull(DeltaTime);
	UpdatePresentation(DeltaTime);
	UpdateCable();
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebug || !GetOwner() || ProbeState.Result == EGrapplingProbeResult::None) return;
	if (HasGrappleAnchor())
	{
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), ProbeState.AnchorPoint, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugSphere(GetWorld(), ProbeState.AnchorPoint, 12.0f, 12, FColor::Green);
		DrawDebugDirectionalArrow(GetWorld(), ProbeState.AnchorPoint, ProbeState.AnchorPoint + ProbeState.SurfaceNormal * 50.0f, 12.0f, FColor::Green);
	}
	else
	{
		DrawDebugLine(GetWorld(), ProbeState.TraceStart, ProbeState.TraceEnd, FColor::Red, false, -1.0f, 0, 2.0f);
		if (ProbeState.Result == EGrapplingProbeResult::Blocked)
		{
			DrawDebugSphere(GetWorld(), ProbeState.TraceEnd, 10.0f, 12, FColor::Red);
		}
	}
#endif
}
