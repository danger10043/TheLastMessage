#include "Character/Player/PlayerAimAnimInstance.h"
#include "Character/Player/PlayerBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "DataAsset/WeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GAS/WeaponGameplayTags.h"

void UPlayerAimAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APlayerBase* Player = Cast<APlayerBase>(TryGetPawnOwner());
	const UWeaponDataAsset* Data = Player ? Player->GetWeaponDataInSlot(Player->GetVisibleWeaponSlot()) : nullptr;
	const bool bRanged = Data && (Data->WeaponType == EWeaponType::LightRanged || Data->WeaponType == EWeaponType::HeavyRanged);
	UAnimSequence* Ready = nullptr;
	if (bRanged)
	{
		Ready = Data->RangedIdleAnimation ? Data->RangedIdleAnimation.Get()
			: (Data->WeaponType == EWeaponType::HeavyRanged ? RifleIdleAnimation : PistolIdleAnimation).LoadSynchronous();
	}
	if (Ready && Ready != RangedReadySequence)
	{
		RangedReadySequence = Ready;
		RangedReadyAlpha = 0.0f;
	}
	const UAnimInstance* MainAnim = GetSkelMeshComponent() ? GetSkelMeshComponent()->GetAnimInstance() : nullptr;
	const bool bReady = Ready && Player && !Player->IsDead() && !Player->IsHitReacting()
		&& !Player->GetAbilitySystemComponent()->HasMatchingGameplayTag(WeaponGameplayTags::State_Grappling);
	float ReadyWeight = bReady ? 1.0f : 0.0f;
	if (bReady && MainAnim && MainAnim->IsAnyMontagePlaying())
	{
		const FAnimMontageInstance* Shot = Data->AttackMontage ? MainAnim->GetInstanceForMontage(Data->AttackMontage) : nullptr;
		// Crossfade back into the held-gun pose during the shot's blend-out, avoiding an unarmed dip.
		ReadyWeight = Shot ? 1.0f - FMath::Clamp(Shot->GetWeight(), 0.0f, 1.0f) : 0.0f;
	}
	RangedReadyAlpha = FMath::FInterpTo(RangedReadyAlpha, ReadyWeight, DeltaSeconds, ReadyInterpolationSpeed);
	const bool bAiming = Player && Player->IsRangedAiming();
	const FVector Aim = Player ? Player->GetAimTargetLocation() - Player->GetActorLocation() : FVector::ForwardVector;
	const FVector Up = Player ? Player->GetActorUpVector() : FVector::UpVector;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Aim, Up),
		FVector::VectorPlaneProject(Aim, Up).Size()));
	SmoothedPitch = FMath::FInterpTo(SmoothedPitch, bAiming ? FMath::Clamp(Pitch, -MaximumAimPitch, MaximumAimPitch) : 0.0f,
		DeltaSeconds, AimInterpolationSpeed);
	RangedAimAlpha = FMath::FInterpTo(RangedAimAlpha, bAiming ? 1.0f : 0.0f, DeltaSeconds, AimInterpolationSpeed);
	if (Player && GetSkelMeshComponent())
	{
		const FVector Axis = GetSkelMeshComponent()->GetComponentTransform().InverseTransformVectorNoScale(Player->GetActorRightVector());
		RangedAimRotation = FQuat(Axis.GetSafeNormal(), -FMath::DegreesToRadians(SmoothedPitch)).Rotator();
	}
}
