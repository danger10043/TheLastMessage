#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "GrapplingHookComponent.generated.h"

class UPrimitiveComponent;
class UAnimMontage;
class UCableComponent;
class USceneComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EGrappleAnimationPhase : uint8
{
	None,
	Throw,
	Pull,
	Land
};

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

// Fixed-anchor grappling. CharacterMovement performs the actual swept movement.
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class THELASTMESSAGE_API UGrapplingHookComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UGrapplingHookComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Player|Grappling")
	void RequestGrapple(FVector WorldAimPoint);
	UFUNCTION(BlueprintCallable, Category="Player|Grappling")
	void ReleaseGrapple();
	// Manual cancellation grants one air jump, unlike arrival/damage/timeout cleanup.
	UFUNCTION(BlueprintCallable, Category="Player|Grappling")
	void CancelGrapple();

	// Authority-side cleanup for death and subsequent movement stages.
	void ClearAnchor();

	UFUNCTION(BlueprintPure, Category="Player|Grappling")
	bool HasGrappleAnchor() const { return ProbeState.Result == EGrapplingProbeResult::Connected; }
	UFUNCTION(BlueprintPure, Category="Player|Grappling")
	bool IsPulling() const { return bMovementOverrideActive; }

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_ProbeState, Category="Player|Grappling")
	FGrapplingProbeState ProbeState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Animation")
	TObjectPtr<UAnimMontage> ThrowMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Animation")
	TObjectPtr<UAnimMontage> PullMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Animation")
	TObjectPtr<UAnimMontage> LandMontage;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Player|Grappling|Animation")
	EGrappleAnimationPhase AnimationPhase = EGrappleAnimationPhase::None;
	// A grapple ending in the air uses normal falling; landing reacts only on real floor contact.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Animation", meta=(ClampMin="0.0", Units="s"))
	float LandingDetectionWindow = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Animation", meta=(ClampMin="0.0", Units="s"))
	float LaunchDelay = 0.08f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Player|Grappling|Cable")
	TObjectPtr<UCableComponent> Cable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Cable")
	FName CableStartSocket = TEXT("SK_Weapon");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Cable", meta=(ClampMin="0.1", Units="cm"))
	float CableWidth = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Cable")
	TObjectPtr<UMaterialInterface> CableMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling", meta=(ClampMin="1.0", Units="cm/s"))
	float PullSpeed = 1200.0f;

	// Additional space outside the capsule, measured from the hit surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling", meta=(ClampMin="1.0", Units="cm"))
	float SurfaceClearance = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling", meta=(ClampMin="0.1", Units="s"))
	float MaxPullDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling", meta=(ClampMin="1.0", Units="cm"))
	float MaxGrappleDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Grappling")
	float ReleaseVelocityValue = 0.75f;

	// The first blocking surface must carry this Actor Tag or Component Tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling")
	FName GrappleableTag = TEXT("Grappleable");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player|Grappling|Debug")
	bool bDrawDebug = false;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestGrapple(FVector_NetQuantize WorldAimPoint);
	UFUNCTION(Server, Reliable)
	void ServerReleaseGrapple();
	UFUNCTION(Server, Reliable)
	void ServerCancelGrapple();
	void CreateCable();
	void UpdateCable();
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CableAnchor;
	UFUNCTION()
	void OnRep_ProbeState();
	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION()
	void OnLanded(const FHitResult& Hit);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartPresentation();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFinishPresentation(bool bCompleted);
	void StopPresentation();
	void UpdatePresentation(float DeltaTime);
	bool PlayGrappleMontage(UAnimMontage* Montage);
	bool CanPlayPresentation() const;
	float GetThrowDuration() const;
	void ProbeForAnchor(FVector WorldAimPoint);
	void BeginPull();
	void EndPull();
	void UpdatePull(float DeltaTime);
	FVector GetPullDestination() const;
	// A press latches the request until arrival or explicit interruption, independent of key-up.
	bool bPullRequested = false;
	bool bMovementOverrideActive = false;
	bool bAddedGrapplingTag = false;
	EMovementMode SavedMovementMode = MOVE_Falling;
	uint8 SavedCustomMovementMode = 0;
	float SavedMaxFlySpeed = 0.0f;
	float PullElapsedTime = 0.0f;
	float StalledTime = 0.0f;
	float LastDistanceToTarget = 0.0f;
	float ThrowDelayRemaining = 0.0f;
	float PresentationThrowRemaining = 0.0f;
	float LandingWaitRemaining = 0.0f;
	bool bPresentationActive = false;
	bool bCompletedPull = false;
};
