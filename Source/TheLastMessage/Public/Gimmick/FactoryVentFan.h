#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FactoryVentFan.generated.h"

class USceneComponent;
class UStaticMesh;

/** Wall-mounted industrial fan. Local Y is the axle; local X/Z form the opening. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API AFactoryVentFan : public AActor
{
    GENERATED_BODY()
public:
    AFactoryVentFan();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fan|Meshes")
    TObjectPtr<UStaticMesh> HousingMesh = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fan|Meshes")
    TObjectPtr<UStaticMesh> BladesMesh = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fan|Meshes")
    TObjectPtr<UStaticMesh> MotorMesh = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fan|Meshes")
    TObjectPtr<UStaticMesh> MountMesh = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fan|Meshes")
    TObjectPtr<UStaticMesh> LampMesh = nullptr;

    UFUNCTION(BlueprintCallable, Category="Fan")
    void SetFanRunning(bool bRunning) { bEnabled = bRunning; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fan")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fan", meta=(Units="RPM"))
    float RevolutionsPerMinute = 45.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fan")
    TObjectPtr<USceneComponent> Rotor;
};
