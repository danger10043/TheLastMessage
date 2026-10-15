#include "Character/Boss/BossMissile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ABossMissile::ABossMissile()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionComponent"));
    SetRootComponent(CollisionComponent);

    // 충돌·피해 처리 구현 전에는 충돌을 활성화하지 않는다.
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CollisionComponent->SetGenerateOverlapEvents(false);

    MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("MissileMesh"));
    MissileMesh->SetupAttachment(CollisionComponent);
    MissileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement =
        CreateDefaultSubobject<UProjectileMovementComponent>(
            TEXT("ProjectileMovement"));
    ProjectileMovement->SetUpdatedComponent(CollisionComponent);
    ProjectileMovement->bAutoActivate = false;
}