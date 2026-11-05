// Copyright Epic Games, Inc. All Rights Reserved.
#include "Camera/SideScrollingCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Map/AreaMapRegion.h"
#include "EngineUtils.h"

namespace
{
/**
 * 값을 [Lo, Hi] 안으로 부드럽게 제한한다. 한계에서 Soft만큼 안쪽부터 점점 느려지며 한계에 닿지 않고 다가간다.
 * 한계 안쪽에서는 값이 그대로 나온다. 범위가 뒤집혀 있으면(방이 화면보다 작을 때) 중앙으로 고정한다.
 */
double SoftClampValue(double Value, double Lo, double Hi, double Soft)
{
	if (Hi <= Lo) return (Lo + Hi) * 0.5;
	Soft = FMath::Clamp(Soft, 0.0, (Hi - Lo) * 0.5);
	if (Soft <= KINDA_SMALL_NUMBER) return FMath::Clamp(Value, Lo, Hi);

	const double UpperStart = Hi - Soft;
	const double LowerStart = Lo + Soft;
	if (Value > UpperStart)
	{
		return UpperStart + Soft * (1.0 - FMath::Exp(-(Value - UpperStart) / Soft));
	}
	if (Value < LowerStart)
	{
		return LowerStart - Soft * (1.0 - FMath::Exp(-(LowerStart - Value) / Soft));
	}
	return Value;
}
}

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
	FVector Goal(FMath::Clamp(Position.X, CameraXMinBounds, CameraXMaxBounds),
		Position.Y + CurrentZoom, CurrentZ + SmoothedFramingOffset);

	// 방(리전)별 카메라 제한: 화면이 방 범위 밖으로 나가지 않게 목표 위치를 부드럽게 제한한다.
	const double HalfWidth = CurrentZoom * FMath::Tan(FMath::DegreesToRadians(OutVT.POV.FOV * 0.5f));
	ApplyRegionConfine(DeltaTime, FVector2D(Position.X, Position.Z), HalfWidth, HalfWidth / Aspect, Goal, bReset);
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

void ASideScrollingCameraManager::RefreshConfineEntries()
{
	ConfineEntries.Reset();
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AAreaMapRegion> It(World); It; ++It)
	{
		AAreaMapRegion* Region = *It;
		if (!IsValid(Region) || !Region->bConfineCamera) continue;
		const FBox2D Bounds = Region->GetMapBounds();
		if (Bounds.GetSize().X < 1.0 || Bounds.GetSize().Y < 1.0) continue;
		FCameraConfineEntry& Entry = ConfineEntries.AddDefaulted_GetRef();
		Entry.Region = Region;
		Entry.Bounds = Bounds;
	}
}

void ASideScrollingCameraManager::SelectConfineRegion(const FVector2D& PlayerPoint)
{
	// 플레이어가 들어 있는 방 중 가장 작은 방을 고른다(큰 방 안의 작은 방이 우선).
	const FCameraConfineEntry* Inside = nullptr;
	for (const FCameraConfineEntry& Entry : ConfineEntries)
	{
		if (!Entry.Region.IsValid() || !Entry.Bounds.IsInside(PlayerPoint)) continue;
		if (!Inside || Entry.Bounds.GetArea() < Inside->Bounds.GetArea()) Inside = &Entry;
	}

	// 지금 방의 경계를 막 넘은 정도에서는 바로 바꾸지 않는다(문 앞에서 왔다 갔다 해도 흔들리지 않게).
	const FCameraConfineEntry* Current = ConfineEntries.FindByPredicate([this](const FCameraConfineEntry& Entry)
	{
		return Entry.Region.IsValid() && Entry.Region.Get() == ActiveConfineRegion.Get();
	});
	if (Current)
	{
		const FBox2D Expanded = Current->Bounds.ExpandBy(FVector2D(ConfineSwitchMargin, ConfineSwitchMargin));
		const bool bNestedSmaller = Inside && Inside != Current && Inside->Bounds.GetArea() < Current->Bounds.GetArea();
		if (Expanded.IsInside(PlayerPoint) && !bNestedSmaller) return;
	}
	if (Inside) ActiveConfineRegion = Inside->Region;
	else ActiveConfineRegion.Reset();
}

void ASideScrollingCameraManager::ApplyConfineAxis(FCameraConfineAxis& Axis, bool bEnabled, double TargetLo, double TargetHi,
	double Soft, double& InOutValue, float DeltaTime, bool bSnap)
{
	if (bEnabled)
	{
		// 제한이 꺼져 있다가 켜질 때는 새 범위로 바로 맞추고, 켜져 있는 동안 방이 바뀌면 범위를 서서히 옮긴다.
		if (bSnap || !Axis.bHasLimits || Axis.Alpha <= KINDA_SMALL_NUMBER)
		{
			Axis.Lo = TargetLo;
			Axis.Hi = TargetHi;
		}
		else
		{
			Axis.Lo = FMath::FInterpTo(Axis.Lo, TargetLo, (double)DeltaTime, (double)ConfineLimitSpeed);
			Axis.Hi = FMath::FInterpTo(Axis.Hi, TargetHi, (double)DeltaTime, (double)ConfineLimitSpeed);
		}
		Axis.Soft = Soft;
		Axis.bHasLimits = true;
	}

	const float TargetAlpha = bEnabled ? 1.f : 0.f;
	if (bSnap)
	{
		Axis.Alpha = TargetAlpha;
	}
	else
	{
		const float Rate = ConfineBlendTime > KINDA_SMALL_NUMBER ? 1.f / ConfineBlendTime : 1000.f;
		Axis.Alpha = FMath::FInterpConstantTo(Axis.Alpha, TargetAlpha, DeltaTime, Rate);
	}

	if (Axis.bHasLimits && Axis.Alpha > 0.f)
	{
		const double Smooth = Axis.Alpha * Axis.Alpha * (3.0 - 2.0 * Axis.Alpha);
		const double Clamped = SoftClampValue(InOutValue, Axis.Lo, Axis.Hi, Axis.Soft);
		InOutValue = FMath::Lerp(InOutValue, Clamped, Smooth);
	}
}

void ASideScrollingCameraManager::ApplyRegionConfine(float DeltaTime, const FVector2D& PlayerPoint, double HalfWidth, double HalfHeight,
	FVector& InOutGoal, bool bSnap)
{
	ConfineRefreshTimer -= DeltaTime;
	if (ConfineRefreshTimer <= 0.f || bSnap)
	{
		ConfineRefreshTimer = 1.0f;
		RefreshConfineEntries();
	}
	SelectConfineRegion(PlayerPoint);

	const AAreaMapRegion* Region = ActiveConfineRegion.Get();
	const FCameraConfineEntry* Entry = Region ? ConfineEntries.FindByPredicate([Region](const FCameraConfineEntry& Candidate)
	{
		return Candidate.Region.Get() == Region;
	}) : nullptr;

	double GoalX = InOutGoal.X, GoalZ = InOutGoal.Z;
	if (Region && Entry)
	{
		// 카메라 중심이 움직일 수 있는 범위: 화면 가장자리가 방 범위(+여유)를 넘지 않는 범위
		const double Overscan = Region->CameraOverscan;
		const double Soft = Region->CameraSoftZone;
		ApplyConfineAxis(ConfineX, Region->bConfineCameraX, Entry->Bounds.Min.X - Overscan + HalfWidth,
			Entry->Bounds.Max.X + Overscan - HalfWidth, Soft, GoalX, DeltaTime, bSnap);
		ApplyConfineAxis(ConfineZ, Region->bConfineCameraZ, Entry->Bounds.Min.Y - Overscan + HalfHeight,
			Entry->Bounds.Max.Y + Overscan - HalfHeight, Soft, GoalZ, DeltaTime, bSnap);
	}
	else
	{
		ApplyConfineAxis(ConfineX, false, 0.0, 0.0, 0.0, GoalX, DeltaTime, bSnap);
		ApplyConfineAxis(ConfineZ, false, 0.0, 0.0, 0.0, GoalZ, DeltaTime, bSnap);
	}
	InOutGoal.X = GoalX;
	InOutGoal.Z = GoalZ;
}
