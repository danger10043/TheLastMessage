#include "Character/Boss/BossRockProjectile.h"
#include "Components/StaticMeshComponent.h"

ABossRockProjectile::ABossRockProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ABossRockProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (IsHidden() || GetNetMode() == NM_DedicatedServer
        || !IsValid(BulletMesh.Get())
        || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f
        || MeshRotationRate.ContainsNaN())
    {
        return;
    }

    BulletMesh->AddLocalRotation(MeshRotationRate * DeltaSeconds);
}