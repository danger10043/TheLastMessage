#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossCircularMissile.generated.h"

class ABossMonster;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInstanceDynamic;
class UGameplayEffect;
class UCurveFloat;

UCLASS()
class THELASTMESSAGE_API ABossCircularMissile : public AActor
{
    GENERATED_BODY()

public:
    ABossCircularMissile();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missile",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.01"))
    float LaunchTime = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missile",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missile",
        meta = (ExposeOnSpawn = "true", ClampMin = "1.0"))
    float DamageRadius = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missile",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.0"))
    float MissileLaunchDelta = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Missile",
        meta = (ExposeOnSpawn = "true"))
    FVector FireLocation = FVector::ZeroVector;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Missile")
    TObjectPtr<UStaticMeshComponent> MissileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Missile")
    TObjectPtr<UNiagaraComponent> TrailVFX;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Missile")
    TObjectPtr<UStaticMeshComponent> WarningSphere;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Missile")
    TObjectPtr<UNiagaraSystem> ExplosionVFX;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Missile")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Missile|Curves")
    TObjectPtr<UCurveFloat> LaunchMoveCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Missile|Curves")
    TObjectPtr<UCurveFloat> AimRotationCurve;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Missile|Curves")
    TObjectPtr<UCurveFloat> ImpactMoveCurve;

private:
    void Explode();
    void ApplyExplosionDamage();

    UPROPERTY(Transient)
    TObjectPtr<ABossMonster> SourceBoss;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> WarningMID;

    FVector StartLocation = FVector::ZeroVector;

    FVector MidLocation = FVector::ZeroVector;

    FVector ImpactLocation = FVector::ZeroVector;

    FQuat LaunchRotation = FQuat::Identity;

    FQuat ImpactRotation = FQuat::Identity;

    double SpawnTime = 0.0;

    float FlightDuration = 0.f;

    bool bExploded = false;
};