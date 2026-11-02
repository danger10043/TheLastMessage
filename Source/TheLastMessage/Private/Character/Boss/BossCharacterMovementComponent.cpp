#include "Character/Boss/BossCharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"

void UBossCharacterMovementComponent::ProcessLanded(
    const FHitResult& Hit, float RemainingTime, int32 Iterations)
{
    if (bJumpAttackActive)
    {
        const UPrimitiveComponent* Surface = Hit.GetComponent();
        if (!Surface || !Hit.bBlockingHit || Velocity.Z > 0.f
            || Surface->GetCollisionObjectType() == ECC_GameTraceChannel1
            || Surface->GetCollisionObjectType() == ECC_Pawn
            || !IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
        {
            return;
        }
    }

    Super::ProcessLanded(Hit, RemainingTime, Iterations);
}