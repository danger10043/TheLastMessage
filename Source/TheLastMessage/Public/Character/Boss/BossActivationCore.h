#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossActivationCore.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPrimitiveComponent;
class UBossCorePuzzleComponent;
class AActor;

UCLASS()
class THELASTMESSAGE_API ABossActivationCore : public AActor
{
    GENERATED_BODY()

public:
    ABossActivationCore();
    void InitializeCore(
        int32 InCoreIndex, UBossCorePuzzleComponent* InPuzzleComponent);
    void HandleMissileHit(AActor* Missile);
    void DeactivateCore();
    void ResetCore();
    bool IsCoreActive();

protected:
    UFUNCTION()
    void OnCoreOverlap(
        UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> CoreMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> CollisionComponent;

private:
    UPROPERTY()
    bool bIsActive = true;

    UPROPERTY()
    int32 CoreIndex = -1;

    UPROPERTY()
    TObjectPtr<UBossCorePuzzleComponent> PuzzleComponent = nullptr;
};