#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class AEnemyBase;
class UNiagaraSystem;
class UNiagaraComponent;

/** 몬스터 공통 투사체 (Cinder, Sentry, Rattler, Juggernaut, Junker가 사용) */
UCLASS()
class THELASTMESSAGE_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyProjectile();

	void InitializeProjectile(AEnemyBase* InSource, float InDamage);
	void LaunchProjectile(FVector Direction);
	void SetProjectileSpeed(float InSpeed);
	void HandleProjectileImpact(AActor* HitActor);
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

	/** 발사체에 붙어 함께 날아가는 플레어 이펙트. BP에서 Niagara System Asset만 지정하면 된다. */
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UNiagaraComponent> FlareEffect;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** 착탄(피해를 주거나 벽에 닿을 때) 이펙트. 비행 중 꼬리 이펙트는 BP에 Niagara 컴포넌트를 직접 붙인다. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(VisibleInstanceOnly, Category = "Projectile")
	float Damage = 0.f;

	UPROPERTY()
	TObjectPtr<AEnemyBase> SourceEnemy;

private:
	bool bImpactHandled = false;
};
