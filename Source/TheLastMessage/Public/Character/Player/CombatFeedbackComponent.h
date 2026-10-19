#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatFeedbackComponent.generated.h"

class UCombatCameraModifier;
class APlayerCameraManager;

USTRUCT(BlueprintType)
struct FCombatFeedbackSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback", meta=(ClampMin="0", ClampMax="0.12", Units="s"))
	float HitStopDuration = 0.045f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback", meta=(ClampMin="0.001", ClampMax="1"))
	float HitStopTimeScale = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback", meta=(ClampMin="0", ClampMax="1", Units="s"))
	float ShakeDuration = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback", meta=(ClampMin="0", Units="cm"))
	float ShakeDistance = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback", meta=(ClampMin="0", Units="deg"))
	float ShakeRotation = 0.25f;
};

/** Actual damage drives feedback; attack presses and immune hits do not. */
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class THELASTMESSAGE_API UCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatFeedbackComponent();
	void NotifyDamageDealt(float Strength = 1.0f);
	void NotifyDamageReceived();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback")
	bool bEnableCameraShake = true;
	// A whole-world stop is used in standalone; network games retain owner-only camera feedback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback")
	bool bEnableHitStop = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback")
	FCombatFeedbackSettings DealtHit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat Feedback")
	FCombatFeedbackSettings ReceivedHit;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void PlayFeedback(bool bReceived, float Strength);
	UFUNCTION(Client, Unreliable)
	void ClientPlayFeedback(bool bReceived, float Strength);
	TWeakObjectPtr<UCombatCameraModifier> CameraModifier;
	TWeakObjectPtr<APlayerCameraManager> CameraManager;
	double LastDealtTime = -1.0;
	double LastReceivedTime = -1.0;
};
