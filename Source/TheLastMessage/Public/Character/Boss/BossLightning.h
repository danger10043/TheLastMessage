#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/PoolableInterface.h"
#include "BossLightning.generated.h"

class ABossMonster;
class APlayerBase;
class UStaticMeshComponent;
class UNiagaraSystem;
class UMaterialInstanceDynamic;
class UGameplayEffect;

UCLASS()
class THELASTMESSAGE_API ABossLightning : public AActor, public IPoolableInterface
{
    GENERATED_BODY()

public:
    ABossLightning();
    virtual void Tick(float DeltaSeconds) override;

    virtual void OnAcquiredFromPool() override;
    virtual void OnReturnedToPool() override;
    bool StartLightning(float InDamage, float InRadius, float InWarningTime);
    void ReturnToPool();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Lightning",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.0"))
    float Damage = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Lightning",
        meta = (ExposeOnSpawn = "true", ClampMin = "1.0"))
    float Radius = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Lightning",
        meta = (ExposeOnSpawn = "true", ClampMin = "0.0"))
    float WarningTime = 1.f;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Lightning")
    TObjectPtr<UStaticMeshComponent> WarningCylinder;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Lightning")
    TObjectPtr<UNiagaraSystem> LightningEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Lightning")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
    void Strike();
    void ApplyStrikeDamage();
    bool IsPlayerInsideCylinder(const APlayerBase* Player) const;

    UPROPERTY(Transient)
    TObjectPtr<ABossMonster> SourceBoss;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> WarningMID;

    FVector StrikeLocation = FVector::ZeroVector;
    double WarningStartTime = 0.0;
    float CylinderHalfHeight = 0.f;
    bool bStruck = false;
    bool bInPool = false;
};