#include "Character/Player/PlayerLocomotionAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gimmick/Component/PushPullComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UPlayerLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
    const bool bFalling = Character && Character->GetCharacterMovement()->IsFalling();
    FVector Left, Right;
    const auto* PushPull = Character ? Character->FindComponentByClass<UPushPullComponent>() : nullptr;
    const bool bGrip = PushPull && PushPull->GetHandContacts(Left, Right);
    BoxGripAlpha = FMath::FInterpConstantTo(BoxGripAlpha, bGrip ? 1.0f : 0.0f, DeltaSeconds, 10.0f);
    if (bGrip && GetSkelMeshComponent())
    {
        const auto* Mesh = GetSkelMeshComponent();
        const FTransform& Transform = Mesh->GetComponentTransform();
        BoxLeftHand = Transform.InverseTransformPosition(Left);
        BoxRightHand = Transform.InverseTransformPosition(Right);
        const FVector Back = -Character->GetActorForwardVector() * 8.0f;
        const FVector Out = Character->GetActorRightVector() * 12.0f;
        const FVector Down = -Character->GetActorUpVector() * 35.0f;
        BoxLeftElbow = Transform.InverseTransformPosition(Mesh->GetSocketLocation(TEXT("upperarm_l")) + Back - Out + Down);
        BoxRightElbow = Transform.InverseTransformPosition(Mesh->GetSocketLocation(TEXT("upperarm_r")) + Back + Out + Down);
    }
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

