#include "Camera/SideScrollingCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void ASideScrollingCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	AActor* Target = OutVT.Target;
	if (!IsValid(Target))
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}

	const FVector ActorLocation = Target->GetActorLocation();
	const FVector CameraLocation = GetCameraLocation();
	const float CameraY = ActorLocation.Y + CurrentZoom;
	OutVT.POV.Rotation = FRotator(0.0f, -90.0f, 0.0f);
	OutVT.POV.FOV = 65.0f;

	if (bSetup)
	{
		bSetup = false;
		// The BP uses the Z offset on initialization, then tracks the stored height.
		OutVT.POV.Location = FVector(ActorLocation.X, CameraY, ActorLocation.Z + CameraZOffset);
		return;
	}

	bool bUpdateHeight;
	APawn* PlayerPawn = PCOwner ? PCOwner->GetPawn().Get() : Cast<APawn>(Target);
	if (!PlayerPawn || PlayerPawn->GetVelocity().Z == 0.0)
	{
		bUpdateHeight = !FMath::IsNearlyEqual(CurrentZ, static_cast<float>(CameraLocation.Z), 25.0f);
	}
	else
	{
		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SideScrollerCamera), false, PlayerPawn);
		const FVector TraceStart = PlayerPawn->GetActorLocation();
		bUpdateHeight = !GetWorld()->LineTraceSingleByChannel(Hit, TraceStart,
			TraceStart + FVector(0, 0, -1000), ECC_Visibility, QueryParams);
	}

	if (bUpdateHeight)
	{
		CurrentZ = ActorLocation.Z;
	}
	else if (!FMath::IsNearlyEqual(CurrentZ, static_cast<float>(ActorLocation.Z), 100.0f))
	{
		CurrentZ = FMath::FInterpTo(CurrentZ, static_cast<float>(ActorLocation.Z), DeltaTime, InterpolationSpeed);
	}

	const FVector Goal(FMath::Clamp(static_cast<float>(ActorLocation.X), CameraXMinBounds, CameraXMaxBounds), CameraY, CurrentZ);
	OutVT.POV.Location = FMath::VInterpTo(CameraLocation, Goal, DeltaTime, InterpolationSpeed);
}
