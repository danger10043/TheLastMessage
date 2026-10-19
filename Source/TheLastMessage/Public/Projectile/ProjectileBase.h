#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectileBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;

UCLASS()
class THELASTMESSAGE_API AProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AProjectileBase();
	bool InitializeProjectile(AActor* Source, TSubclassOf<UGameplayEffect> Effect, float InDamage, float Speed, float Range);

	void LaunchProjectile(const FVector& Direction);
	void DestroyProjectile();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleInstanceOnly, Category = "Projectile")
	float Damage = 0.f;

private:
	void HandleImpact(AActor* OtherActor, bool bBlocking);
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SourceActor;
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	float MaxTravelTime = 0.0f;
	bool bLaunched = false;
	bool bImpactHandled = false;
};
