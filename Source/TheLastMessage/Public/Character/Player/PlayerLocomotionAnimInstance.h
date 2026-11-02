#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerLocomotionAnimInstance.generated.h"

/** Keeps an airborne weapon montage masked through its landing and blend out. */
UCLASS(Transient, Blueprintable)
class THELASTMESSAGE_API UPlayerLocomotionAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Interaction")
    FVector BoxLeftHand = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Interaction")
    FVector BoxRightHand = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Interaction")
    FVector BoxLeftElbow = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Interaction")
    FVector BoxRightElbow = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Interaction")
    float BoxGripAlpha = 0.0f;
protected:
    UPROPERTY(BlueprintReadOnly, Transient, Category="Weapon|Animation")
    bool bUseAirAttackPose = false;
private:
    TWeakObjectPtr<UAnimMontage> AirAttackMontage;
    int32 LastAttackInstanceID = INDEX_NONE;
};
