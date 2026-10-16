#include "Character/Player/GrapplingHookComponent.h"
#include "Character/Player/PlayerBase.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UGrapplingHookComponent::UGrapplingHookComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UGrapplingHookComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGrapplingHookComponent, ProbeState);
}

void UGrapplingHookComponent::RequestGrapple(FVector WorldAimPoint)
{
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority())
	{
		ProbeForAnchor(WorldAimPoint);
	}
	else if (const APawn* Pawn = Cast<APawn>(GetOwner()); Pawn && Pawn->IsLocallyControlled())
	{
		ServerRequestGrapple(WorldAimPoint);
	}
}

void UGrapplingHookComponent::ServerRequestGrapple_Implementation(FVector_NetQuantize WorldAimPoint)
{
	ProbeForAnchor(WorldAimPoint);
}

void UGrapplingHookComponent::ClearAnchor()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ProbeState = FGrapplingProbeState();
		GetOwner()->ForceNetUpdate();
	}
}

void UGrapplingHookComponent::ProbeForAnchor(FVector WorldAimPoint)
{
	const APlayerBase* Player = Cast<APlayerBase>(GetOwner());
	if (!Player || Player->IsDead() || Player->IsHitReacting() || WorldAimPoint.ContainsNaN()) return;
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
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrapplingProbe), false, Player);
	// Ignore anything attached to the character, including future weapon actors.
	TArray<AActor*> AttachedActors;
	Player->GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);
	ProbeState.TraceStart = Start;
	ProbeState.TraceEnd = bHit ? Hit.ImpactPoint : End;
	ProbeState.Result = bHit ? EGrapplingProbeResult::Blocked : EGrapplingProbeResult::Miss;
	if (bHit && !Hit.bStartPenetrating && !GrappleableTag.IsNone()
		&& ((Hit.GetActor() && Hit.GetActor()->ActorHasTag(GrappleableTag))
			|| (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(GrappleableTag))))
	{
		ProbeState.Result = EGrapplingProbeResult::Connected;
		ProbeState.AnchorPoint = Hit.ImpactPoint;
		ProbeState.SurfaceNormal = Hit.ImpactNormal;
	}
	GetOwner()->ForceNetUpdate();
}

void UGrapplingHookComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
