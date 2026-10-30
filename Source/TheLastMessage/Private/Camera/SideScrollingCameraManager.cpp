// Copyright Epic Games, Inc. All Rights Reserved.
#include "Camera/SideScrollingCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

void ASideScrollingCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	APawn* Pawn = Cast<APawn>(OutVT.Target);
	if (!IsValid(Pawn))
	{
		bSetup = true;
		FollowedPawn.Reset();
		Super::UpdateViewTarget(OutVT, DeltaTime);
		return;
	}

	OutVT.POV.Rotation = FRotator(0.0f, -90.0f, 0.0f);
	OutVT.POV.FOV = 65.0f;
	const FVector Position = Pawn->GetActorLocation();
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const FVector Gravity = Movement ? Movement->GetGravityDirection() : FVector(0, 0, -1);
	const float UpSign = Gravity.Z > 0.0f ? -1.0f : 1.0f;
	const bool bGrounded = Movement && Movement->IsMovingOnGround();
	const bool bReset = bSetup || FollowedPawn.Get() != Pawn
		|| FVector::DistSquared(Position, PreviousTargetLocation) > FMath::Square(2500.0f);

	int32 Width = 0, Height = 0;
	if (PCOwner) PCOwner->GetViewportSize(Width, Height);
	const float Aspect = Width > 0 && Height > 0 ? float(Width) / Height : 16.0f / 9.0f;
	const float ViewHeight = 2.0f * FMath::Max(CurrentZoom, 1.0f)
		* FMath::Tan(FMath::DegreesToRadians(OutVT.POV.FOV * 0.5f)) / Aspect;
	float DesiredOffset = UpSign * FMath::Max(CameraZOffset, (PlayerScreenHeight - 0.5f) * ViewHeight);

	// Probe beyond the current support, rather than treating every platform as the bottom of a level.
	// Only physical world geometry counts; enemies and effects cannot change the framing.
	TerrainProbeTimer -= DeltaTime;
	if (bGrounded && (bReset || TerrainProbeTimer <= 0.0f))
	{
		TerrainProbeTimer = 0.15f;
		bHasLowerTerrain = false;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraLowerTerrain), false, Pawn);
		Params.AddIgnoredComponent(Movement->CurrentFloor.HitResult.GetComponent());
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		const FVector Floor = Movement->CurrentFloor.HitResult.ImpactPoint;
		for (const float OffsetX : {-0.35f, 0.0f, 0.35f})
		{
			const FVector Start = Floor + FVector(OffsetX * ViewHeight, 0, 0) + Gravity * 30.0f;
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Gravity * ViewHeight * 0.7f, Objects, Params)
				&& FVector::DotProduct(Hit.ImpactNormal, -Gravity) > 0.5f)
			{
				bHasLowerTerrain = true;
				break;
			}
		}
	}
	if (bGrounded && !bHasLowerTerrain)
	{
		const float FloorZ = Movement->CurrentFloor.HitResult.ImpactPoint.Z;
		const float FloorOffset = (0.5f - GroundBackgroundFraction) * ViewHeight;
		DesiredOffset = UpSign * FMath::Max(FMath::Abs(DesiredOffset),
			FloorOffset - UpSign * (Position.Z - FloorZ));
	}

	if (bReset)
	{
		CurrentZ = Position.Z;
		SmoothedFramingOffset = DesiredOffset;
	}
	else
	{
		// Small jumps stay inside a vertical window; landing recenters smoothly.
		const float DeadZone = ViewHeight * VerticalDeadZoneFraction;
		CurrentZ = bGrounded ? Position.Z : FMath::Clamp(CurrentZ, Position.Z - DeadZone, Position.Z + DeadZone);
		SmoothedFramingOffset = FMath::FInterpTo(SmoothedFramingOffset, DesiredOffset, DeltaTime, 2.0f);
	}
	const FVector Goal(FMath::Clamp(Position.X, CameraXMinBounds, CameraXMaxBounds),
		Position.Y + CurrentZoom, CurrentZ + SmoothedFramingOffset);
	if (bReset) OutVT.POV.Location = Goal;
	else
	{
		OutVT.POV.Location = FMath::VInterpTo(UnmodifiedCameraLocation, Goal, DeltaTime, 2.0f);
		OutVT.POV.Location.Z = FMath::FInterpTo(UnmodifiedCameraLocation.Z, Goal.Z, DeltaTime, VerticalFollowSpeed);
	}
	bSetup = false;
	FollowedPawn = Pawn;
	PreviousTargetLocation = Position;
	FinalizeViewTarget(OutVT, DeltaTime);
}

void ASideScrollingCameraManager::FinalizeViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	UnmodifiedCameraLocation = OutVT.POV.Location;
	ApplyCameraModifiers(DeltaTime, OutVT.POV);
	SetActorLocationAndRotation(OutVT.POV.Location, OutVT.POV.Rotation, false);
	UpdateCameraLensEffects(OutVT);
}
