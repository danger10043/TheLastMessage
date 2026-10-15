#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossMissile.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UProjectileMovementComponent;
class ABossMonster;

UCLASS()
class THELASTMESSAGE_API ABossMissile : public AActor
{
    GENERATED_BODY()

public:
    ABossMissile();
    void InitializeMissile(ABossMonster* InBoss, float InDamage);
    void LaunchMissile(FVector Direction);
    void SetTargetLocation(FVector NewLocation);
    void HandleMissileImpact(AActor* HitActor);
    bool CanDeactivateCore();
    void DestroyMissile();

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MissileMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    UPROPERTY()
    float Damage = 0.f;

    UPROPERTY()
    TObjectPtr<ABossMonster> SourceBoss = nullptr;

    UPROPERTY()
    bool bCanDeactivateCore = true;

    UPROPERTY()
    FVector TargetLocation = FVector::ZeroVector;
};