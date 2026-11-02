#include "Character/Player/PlayerLocomotionAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UPlayerLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
    const bool bFalling = Character && Character->GetCharacterMovement()->IsFalling();
    // Keep the latch until weight reaches zero, including cancellation blend outs.
    if (AirAttackMontage.IsValid() && Montage_GetIsStopped(AirAttackMontage.Get())
        && !Montage_IsPlaying(AirAttackMontage.Get()))
    {
        const FAnimMontageInstance* Instance = GetInstanceForMontage(AirAttackMontage.Get());
        if (!Instance || Instance->GetWeight() <= ZERO_ANIMWEIGHT_THRESH) AirAttackMontage.Reset();
    }
    UAnimMontage* Active = GetCurrentActiveMontage();
    if (Active && (Active->GetName().StartsWith(TEXT("AM_PlayerAttack_"))
        || Active->GetName().StartsWith(TEXT("AM_PlayerCharge_"))))
    {
        const FAnimMontageInstance* Instance = GetActiveInstanceForMontage(Active);
        const int32 InstanceID = Instance ? Instance->GetInstanceID() : INDEX_NONE;
        if (LastAttackInstanceID != InstanceID)
        {
            const bool bAirChargeRelease = AirAttackMontage.IsValid()
                && AirAttackMontage->GetName().StartsWith(TEXT("AM_PlayerCharge_"))
                && Active->GetName().StartsWith(TEXT("AM_PlayerAttack_"));
            if (bAirChargeRelease) AirAttackMontage = Active;
            else AirAttackMontage.Reset();
        }
        LastAttackInstanceID = InstanceID;
        if (bFalling) AirAttackMontage = Active;
        else if (AirAttackMontage.Get() != Active) AirAttackMontage.Reset();
    }
    else if (Active) AirAttackMontage.Reset(); // Hit/death montages retain their full-body pose.
    bUseAirAttackPose = AirAttackMontage.IsValid();
}

