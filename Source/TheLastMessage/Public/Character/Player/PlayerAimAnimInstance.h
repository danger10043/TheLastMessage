#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAimAnimInstance.generated.h"

class UAnimSequence;

/** Post-process animation: adds upper-body aim to the existing completed pose. */
UCLASS(Transient, Blueprintable)
class THELASTMESSAGE_API UPlayerAimAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Player|Ranged Idle")
	TObjectPtr<UAnimSequence> RangedReadySequence;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Player|Ranged Idle")
	float RangedReadyAlpha = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player|Ranged Idle")
	TObjectPtr<UAnimSequence> PistolIdleAnimation = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player|Ranged Idle")
	TObjectPtr<UAnimSequence> RifleIdleAnimation = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player|Ranged Idle", meta=(ClampMin="0.1"))
	float ReadyInterpolationSpeed = 16.0f;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Player|Ranged Aim")
	FRotator RangedAimRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Player|Ranged Aim")
	float RangedAimAlpha = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player|Ranged Aim", meta=(ClampMin="0", ClampMax="89"))
	float MaximumAimPitch = 80.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Player|Ranged Aim", meta=(ClampMin="0.1"))
	float AimInterpolationSpeed = 15.0f;
private:
	float SmoothedPitch = 0.0f;
};
