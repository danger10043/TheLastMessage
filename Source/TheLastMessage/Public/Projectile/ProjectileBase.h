#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "ProjectileBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class UWeaponDataAsset;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS(Blueprintable)
class THELASTMESSAGE_API AProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AProjectileBase();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ConfigureVisuals(UWeaponDataAsset* Data);
	float GetProjectileCollisionRadius() const;
	bool InitializeProjectile(AActor* Source, TSubclassOf<UGameplayEffect> Effect, float InDamage, float Speed, float Range, float InFeedbackStrength = 1.0f);

	void LaunchProjectile(const FVector& Direction);
	void DestroyProjectile();

protected:
	virtual void BeginPlay() override;
	// Disable to author mesh/material/scale directly on the projectile Blueprint.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Visual")
	bool bUseWeaponVisualSettings = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Visual")
	FLinearColor ProjectileTint = FLinearColor(0.02f, 0.65f, 1.0f);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile|Visual")
	TObjectPtr<UNiagaraComponent> FlightEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Impact")
	TObjectPtr<USoundBase> ImpactSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Travel")
	bool bOverrideWeaponRange = false;
	// Centimeters. When enabled this replaces the weapon data's Range.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Travel", meta=(ClampMin="1", EditCondition="bOverrideWeaponRange"))
	float TravelRange = 2000.0f;

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
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector_NetQuantize Location);
	UFUNCTION()
	void OnRep_VisualData();
	UPROPERTY(ReplicatedUsing=OnRep_VisualData)
	TObjectPtr<UWeaponDataAsset> VisualData;
	void HandleImpact(AActor* OtherActor, bool bBlocking);
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SourceActor;
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	float MaxTravelTime = 0.0f;
	float FeedbackStrength = 1.0f;
	bool bLaunched = false;
	bool bImpactHandled = false;
};
