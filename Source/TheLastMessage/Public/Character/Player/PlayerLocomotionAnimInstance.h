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
protected:
    UPROPERTY(BlueprintReadOnly, Transient, Category="Weapon|Animation")
    bool bUseAirAttackPose = false;
private:
    TWeakObjectPtr<UAnimMontage> AirAttackMontage;
    int32 LastAttackInstanceID = INDEX_NONE;
};
