#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerCheckpoint.generated.h"

class APlayerBase;
class UBoxComponent;
class UArrowComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCheckpointActivated, APlayerBase*, Player);

/** Checkpoint actor; the arrow marks the respawn capsule center and facing. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API APlayerCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	APlayerCheckpoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	TObjectPtr<USceneComponent> RespawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	TObjectPtr<UArrowComponent> RespawnArrow;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Checkpoint")
	bool ActivateCheckpoint(APlayerBase* Player);

	/** Runs on the authority; use a Blueprint for optional visuals/audio. */
	UPROPERTY(BlueprintAssignable, Category="Checkpoint")
	FCheckpointActivated OnCheckpointActivated;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
