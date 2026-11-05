#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DirectionalStairs.generated.h"

class ACharacter;
class UStaticMeshComponent;

/** Local -X is the upper end; walking from that end towards +X can pass underneath. */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ADirectionalStairs : public AActor
{
	GENERATED_BODY()
public:
	ADirectionalStairs();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stairs")
	TObjectPtr<UStaticMeshComponent> StairMesh;
	/** Permanent collision for movement, grapple probes and projectiles. Never disabled per character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stairs")
	TObjectPtr<UStaticMeshComponent> SolidRamp;

	UFUNCTION(BlueprintPure, Category="Stairs")
	static bool ShouldAllowWalkThrough(FVector LocalPosition, FVector LocalVelocity,
		bool bWalking, bool bGrappling, bool bAlreadyPassing);
	UFUNCTION(BlueprintCallable, Category="Stairs")
	bool RequestDropThrough(ACharacter* Character);

private:
	TSet<TWeakObjectPtr<ACharacter>> PassingCharacters;
	TSet<TWeakObjectPtr<ACharacter>> DroppingCharacters;
	TSet<TWeakObjectPtr<ACharacter>> OrderedCharacters;
};
