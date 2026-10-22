#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossActivationCore.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UBossCorePuzzleComponent;
class UNiagaraSystem;

UCLASS()
class THELASTMESSAGE_API ABossActivationCore : public AActor
{
    GENERATED_BODY()

public:
    ABossActivationCore();
    virtual void Tick(float DeltaSeconds) override;

    bool CanActivateFor(
        const UBossCorePuzzleComponent* InPuzzleComponent) const;
    bool InitializeCore(
        int32 InCoreIndex, UBossCorePuzzleComponent* InPuzzleComponent);

    void HandleMissileHit(AActor* Missile);
    void DeactivateCore();
    void ResetCore();

    bool IsCoreActive() const { return bIsActive; }
    bool IsBroken() const { return bIsBroken; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Core")
    TObjectPtr<USkeletalMeshComponent> CoreMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Core")
    TObjectPtr<USphereComponent> CollisionComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Core|Animation")
    TObjectPtr<UAnimMontage> SpawnMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Core|Animation")
    TObjectPtr<UAnimMontage> RetractMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Core|Animation")
    TObjectPtr<UAnimMontage> BreakMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss|Core|Effects")
    TObjectPtr<UNiagaraSystem> BreakEffect;

private:
    enum class ECoreState : uint8
    {
        Hidden,
        Spawning,
        Active,
        Retracting,
        Breaking,
        Fading
    };

    bool PlayCoreMontage(UAnimMontage* Montage);
    void HideCore();
    void SetCoreOpacity(float Opacity);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

    UPROPERTY()
    TObjectPtr<UBossCorePuzzleComponent> PuzzleComponent = nullptr;

    ECoreState CoreState = ECoreState::Hidden;
    bool bIsActive = false;
    bool bIsBroken = false;
    int32 CoreIndex = INDEX_NONE;
    int32 MontageInstanceID = INDEX_NONE;
    float FadeElapsed = 0.f;
};