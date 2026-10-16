#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "GrapplingHookComponent.generated.h"

UENUM(BlueprintType)
enum class EGrapplingProbeResult : uint8
{
	None,
	Connected,
	Blocked,
	Miss
};

USTRUCT(BlueprintType)
struct FGrapplingProbeState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	EGrapplingProbeResult Result = EGrapplingProbeResult::None;
	UPROPERTY(BlueprintReadOnly)
	FVector TraceStart = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly)
	FVector TraceEnd = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly)
	FVector AnchorPoint = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly)
	FVector SurfaceNormal = FVector::ZeroVector;
};

// First grappling stage: probe and store a fixed world anchor, without moving the player.
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class THELASTMESSAGE_API UGrapplingHookComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UGrapplingHookComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Player|Grappling")
	void RequestGrapple(FVector WorldAimPoint);

	// Authority-side cleanup for death and subsequent movement stages.
	void ClearAnchor();

	UFUNCTION(BlueprintPure, Category="Player|Grappling")
	bool HasGrappleAnchor() const { return ProbeState.Result == EGrapplingProbeResult::Connected; }

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Player|Grappling")
	FGrapplingProbeState ProbeState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling", meta=(ClampMin="1.0", Units="cm"))
	float MaxGrappleDistance = 1500.0f;

	// The first blocking surface must carry this Actor Tag or Component Tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling")
	FName GrappleableTag = TEXT("Grappleable");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Debug")
	bool bDrawDebug = true;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestGrapple(FVector_NetQuantize WorldAimPoint);
	void ProbeForAnchor(FVector WorldAimPoint);
};
