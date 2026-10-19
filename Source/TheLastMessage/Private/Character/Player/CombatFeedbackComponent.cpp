#include "Character/Player/CombatFeedbackComponent.h"
#include "Character/Player/CombatCameraModifier.h"
#include "Character/Player/CombatHitStopSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

UCombatFeedbackComponent::UCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReceivedHit.HitStopDuration = 0.065f;
	ReceivedHit.ShakeDuration = 0.22f;
	ReceivedHit.ShakeDistance = 3.5f;
	ReceivedHit.ShakeRotation = 0.45f;
}

void UCombatFeedbackComponent::NotifyDamageDealt(float Strength)
{
	PlayFeedback(false, Strength);
}

void UCombatFeedbackComponent::NotifyDamageReceived()
{
	PlayFeedback(true, 1.0f);
}

void UCombatFeedbackComponent::PlayFeedback(bool bReceived, float Strength)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()
		|| !FMath::IsFinite(Strength) || Strength <= 0) return;
	double& LastTime = bReceived ? LastReceivedTime : LastDealtTime;
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now - LastTime < 0.04) return;
	LastTime = Now;
	Strength = FMath::Clamp(Strength, 0.25f, 2.0f);
	const FCombatFeedbackSettings& Settings = bReceived ? ReceivedHit : DealtHit;
	if (bEnableHitStop)
	{
		if (UCombatHitStopSubsystem* Stop = GetWorld()->GetSubsystem<UCombatHitStopSubsystem>())
			Stop->RequestHitStop(Settings.HitStopDuration * Strength, Settings.HitStopTimeScale);
	}
	ClientPlayFeedback(bReceived, Strength);
}

void UCombatFeedbackComponent::ClientPlayFeedback_Implementation(bool bReceived, float Strength)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!bEnableCameraShake || !PC || !PC->IsLocalController() || !PC->PlayerCameraManager) return;
	APlayerCameraManager* Manager = PC->PlayerCameraManager;
	if (CameraManager.Get() != Manager || !CameraModifier.IsValid())
	{
		if (CameraManager.IsValid() && CameraModifier.IsValid()) CameraManager->RemoveCameraModifier(CameraModifier.Get());
		CameraManager = Manager;
		CameraModifier = Cast<UCombatCameraModifier>(Manager->AddNewCameraModifier(UCombatCameraModifier::StaticClass()));
	}
	if (CameraModifier.IsValid()) CameraModifier->PlayImpulse(bReceived ? ReceivedHit : DealtHit, Strength);
}

void UCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CameraManager.IsValid() && CameraModifier.IsValid()) CameraManager->RemoveCameraModifier(CameraModifier.Get());
	Super::EndPlay(EndPlayReason);
}
