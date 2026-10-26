#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerCheckpoint.generated.h"

class APlayerBase;
class UBoxComponent;
class UArrowComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ECheckpointVisualState : uint8
{
	Inactive UMETA(DisplayName="Inactive / Unvisited"),
	Current UMETA(DisplayName="Active / Latest Checkpoint"),
	Visited UMETA(DisplayName="Active / Previously Visited")
};

USTRUCT(BlueprintType)
struct FCheckpointSignalMaterials
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TObjectPtr<UMaterialInterface> Screen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TObjectPtr<UMaterialInterface> Ring;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TObjectPtr<UMaterialInterface> Core;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TObjectPtr<UMaterialInterface> Beacon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TObjectPtr<UMaterialInterface> BeaconBand;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCheckpointActivated, APlayerBase*, Player);

/** Checkpoint actor; the arrow marks the respawn capsule center and facing. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API APlayerCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	APlayerCheckpoint();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	TObjectPtr<USceneComponent> RespawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	TObjectPtr<UArrowComponent> RespawnArrow;

	/** Front is local +Y. Single mesh, 344 cm tall, with nine material slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint|Visual")
	TObjectPtr<USceneComponent> TerminalRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint|Visual")
	TObjectPtr<UStaticMeshComponent> TerminalBody;

	/** Editable for previewing all three states without playing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_VisualState, Category="Checkpoint|Visual")
	ECheckpointVisualState CheckpointVisualState = ECheckpointVisualState::Inactive;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Checkpoint|Visual|Materials")
	FCheckpointSignalMaterials InactiveMaterials;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Checkpoint|Visual|Materials")
	FCheckpointSignalMaterials CurrentMaterials;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Checkpoint|Visual|Materials")
	FCheckpointSignalMaterials VisitedMaterials;

	/** Compatibility for existing Blueprint readers: true for Current or Visited. */
	UPROPERTY(BlueprintReadOnly, Category="Checkpoint|Visual", meta=(DeprecatedProperty, DeprecationMessage="Use CheckpointVisualState"))
	bool bCheckpointActivated = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Checkpoint|Visual", meta=(DeprecatedFunction, DeprecationMessage="Use SetCheckpointVisualState"))
	void SetCheckpointVisualActive(bool bActive);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Checkpoint|Visual")
	void SetCheckpointVisualState(ECheckpointVisualState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Checkpoint")
	bool ActivateCheckpoint(APlayerBase* Player);

	void SetInteractionHighlight(bool bVisible);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint|Visual")
	TObjectPtr<UStaticMeshComponent> InteractionOutline;

	/** Runs on the authority; use a Blueprint for optional visuals/audio. */
	UPROPERTY(BlueprintAssignable, Category="Checkpoint")
	FCheckpointActivated OnCheckpointActivated;

private:
	TSet<TWeakObjectPtr<APlayerBase>> TouchingPlayers;
	void RefreshTerminalVisuals();
	UFUNCTION()
	void OnRep_VisualState();

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

};
