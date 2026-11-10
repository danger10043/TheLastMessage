#include "Gimmick/DirectionalStairs.h"

#include "Character/Player/PlayerBase.h"
#include "Character/Player/GrapplingHookComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ADirectionalStairs::ADirectionalStairs()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	StairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StairMesh"));
	SetRootComponent(StairMesh);
	StairMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StairMesh->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> StairsAsset(TEXT("/Game/Main/WorldAssets/Meshes/SM_Stairs.SM_Stairs"));
	if (StairsAsset.Succeeded()) StairMesh->SetStaticMesh(StairsAsset.Object);

	SolidRamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SolidRamp"));
	SolidRamp->SetupAttachment(StairMesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded()) SolidRamp->SetStaticMesh(CubeAsset.Object);
	// A smooth, thin ramp underneath the treads. This is a mesh so grapple probes
	// also recognize it in maps using either authored or automatic hook collision.
	SolidRamp->SetRelativeLocation(FVector(-125, 0, 93));
	SolidRamp->SetRelativeRotation(FRotator(-29.744881f, 0, 0));
	SolidRamp->SetRelativeScale3D(FVector(4.031129f, 4, 0.12f));
	SolidRamp->SetCollisionProfileName(TEXT("BlockAll"));
	SolidRamp->SetCollisionObjectType(ECC_WorldStatic);
	SolidRamp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SolidRamp->SetCollisionResponseToAllChannels(ECR_Block);
	SolidRamp->SetGenerateOverlapEvents(false);
	SolidRamp->SetHiddenInGame(true);
	SolidRamp->SetVisibility(false);
	SolidRamp->SetCanEverAffectNavigation(true);
}

bool ADirectionalStairs::ShouldAllowWalkThrough(FVector P, FVector V,
	bool bWalking, bool bGrappling, bool bAlreadyPassing)
{
	if (!bWalking || bGrappling || P.ContainsNaN() || V.ContainsNaN()) return false;
	// Keep the exception until the capsule is clear of the low end. Do not
	// restore halfway through the slope and trap a character inside it.
	if (P.X < -500 || P.X > 200 || FMath::Abs(P.Y) > 350 || P.Z < -200 || P.Z > 300) return false;
	if (bAlreadyPassing) return true;
	const float SurfaceZ = (50.0f - P.X) * (200.0f / 350.0f);
	// After dropping onto the floor, either neighboring module can overlap the
	// capsule. Entering its underside must work in both horizontal directions;
	// approaching the walkable top from the low end still fails the height test.
	// A blocked movement sweep can zero horizontal velocity before the next tick.
	// Being underneath is sufficient; requiring speed would deadlock that contact.
	return P.Z < SurfaceZ - 20;
}

bool ADirectionalStairs::RequestDropThrough(ACharacter* Character)
{
	if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
	if (Character->GetCharacterMovement()->CurrentFloor.HitResult.GetActor() != this) return false;
	// At a modular stair seam the capsule can touch both ramps. Ignoring only
	// the supporting actor leaves the neighboring ramp trapping the falling capsule.
	const FBox DropBounds = Character->GetCapsuleComponent()->Bounds.GetBox().ExpandBy(10.f);
	for (TActorIterator<ADirectionalStairs> It(GetWorld()); It; ++It)
	{
		ADirectionalStairs* Stairs = *It;
		if (Stairs != this && !Stairs->SolidRamp->Bounds.GetBox().Intersect(DropBounds)) continue;
		Stairs->DroppingCharacters.Add(Character);
		Stairs->PassingCharacters.Add(Character);
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Stairs, true);
	}
	Character->StopJumping();
	Character->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Character->GetCharacterMovement()->Velocity.Z = FMath::Min(Character->GetVelocity().Z, -100.0);
	return true;
}

void ADirectionalStairs::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Character = *It;
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		if (!Movement || !Capsule) continue;
		if (!OrderedCharacters.Contains(Character))
		{
			// Evaluate after the hook changes movement mode, before the movement sweep.
			if (auto* Hook = Character->FindComponentByClass<UGrapplingHookComponent>()) AddTickPrerequisiteComponent(Hook);
			Movement->AddTickPrerequisiteActor(this);
			OrderedCharacters.Add(Character);
		}
		const APlayerBase* Player = Cast<APlayerBase>(Character);
		const bool bWasPassing = PassingCharacters.Contains(Character);
		const FVector LocalPosition = GetActorTransform().InverseTransformPosition(Capsule->GetComponentLocation());
		const bool bGrappling = Player && Player->IsGrappling();
		bool bDropping = DroppingCharacters.Contains(Character);
		const bool bWasDropping = bDropping;
		if (bDropping)
		{
			const FVector Head = GetActorTransform().InverseTransformPosition(Capsule->GetComponentLocation() + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()));
			const float SurfaceZ = (50.0f - LocalPosition.X) * (200.0f / 350.0f);
			const bool bLandedElsewhere = Movement->IsMovingOnGround() && Movement->CurrentFloor.HitResult.GetActor() != this;
			const bool bCapsuleClear = !SolidRamp->Bounds.GetBox().Intersect(Capsule->Bounds.GetBox().ExpandBy(2.f));
			const double Clearance = (SurfaceZ - Head.Z) * GetActorScale3D().Z;
			if (bGrappling || (bLandedElsewhere && bCapsuleClear) || Clearance > 30.0 || LocalPosition.X < -500 || LocalPosition.X > 200 || FMath::Abs(LocalPosition.Y) > 350)
			{
				DroppingCharacters.Remove(Character);
				bDropping = false;
			}
		}
		const FVector Feet = GetActorTransform().InverseTransformPosition(Capsule->GetComponentLocation() - FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()));
		const bool bAbove = Feet.Z > (50.0f - LocalPosition.X) * (200.0f / 350.0f) + 20;
		const bool bAllow = bDropping || (!bWasDropping && ShouldAllowWalkThrough(
			LocalPosition,
			GetActorTransform().InverseTransformVector(Character->GetVelocity()),
			Movement->IsWalking() || Movement->IsFalling(), bGrappling, bWasPassing && !bAbove));
		if (bAllow != bWasPassing)
		{
			// Only this character's movement ignores the stairs. Weapon traces and
			// projectile components retain their own, permanently blocking collision.
			Capsule->IgnoreActorWhenMoving(this, bAllow);
			if (bAllow) PassingCharacters.Add(Character);
			else PassingCharacters.Remove(Character);
		}
	}
	for (auto It = PassingCharacters.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
}

void ADirectionalStairs::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const auto& Weak : OrderedCharacters)
	{
		if (ACharacter* Character = Weak.Get())
		{
			Character->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);
			Character->GetCharacterMovement()->RemoveTickPrerequisiteActor(this);
		}
	}
	PassingCharacters.Empty();
	DroppingCharacters.Empty();
	OrderedCharacters.Empty();
	Super::EndPlay(Reason);
}
