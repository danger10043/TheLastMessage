#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/PoolableInterface.h"
#include "TimerManager.h"
#include "BossBullet.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class UProjectileMovementComponent;

UCLASS()
class THELASTMESSAGE_API ABossBullet : public AActor, public IPoolableInterface
{
    GENERATED_BODY()

public:
    ABossBullet();


    virtual void OnAcquiredFromPool() override;
    virtual void OnReturnedToPool() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Pool -> configure -> launch. BeginPlay no longer launches automatically.
    bool StartBullet(float InDamage, float InSpeed, float Lifetime);
    void ReturnToPool();

private:
    UFUNCTION()
    void OnRep_FlightState();

    UPROPERTY(ReplicatedUsing = OnRep_FlightState)
    bool bFlightActive = false;

    UPROPERTY(ReplicatedUsing = OnRep_FlightState)
    uint32 FlightSerial = 0;

    bool bInPool = true;
    FTimerHandle FlightTimer;

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Bullet",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Bullet",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.0", Units = "cm/s"))
    float Speed = 1500.f;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Bullet")
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Bullet")
    TObjectPtr<UStaticMeshComponent> BulletMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Bullet")
    TObjectPtr<UNiagaraSystem> BulletTrail;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Bullet")
    TObjectPtr<UNiagaraSystem> BulletStrike;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Bullet")
    TObjectPtr<UNiagaraComponent> TrailComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Bullet")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
    bool bReady = false;
    bool bImpactHandled = false;

    UFUNCTION()
    void OnProjectileStop(const FHitResult& Hit);

    UFUNCTION()
    void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    void HandleImpact(AActor* OtherActor, const FHitResult& Hit);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStrike(FVector Location, FRotator Rotation);
};