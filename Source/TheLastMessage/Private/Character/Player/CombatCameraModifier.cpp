#include "Character/Player/CombatCameraModifier.h"
#include "Character/Player/CombatFeedbackComponent.h"
#include "Engine/World.h"

void UCombatCameraModifier::PlayImpulse(const FCombatFeedbackSettings& Settings, float Strength)
{
	if (!GetWorld() || !FMath::IsFinite(Strength) || Strength <= 0) return;
	const double Now = GetWorld()->GetRealTimeSeconds();
	const float Remaining = FMath::Clamp(1.0f - static_cast<float>((Now - StartRealTime) / FMath::Max(Duration, 0.001f)), 0.0f, 1.0f);
	// Replace or strengthen a pulse; multi-target hits never sum unbounded amplitudes.
	Distance = FMath::Max(Distance * Remaining * Remaining, FMath::Clamp(Settings.ShakeDistance, 0.0f, 20.0f) * Strength);
	Rotation = FMath::Max(Rotation * Remaining * Remaining, FMath::Clamp(Settings.ShakeRotation, 0.0f, 3.0f) * Strength);
	Duration = FMath::Clamp(Settings.ShakeDuration, 0.0f, 1.0f);
	StartRealTime = Now;
}

bool UCombatCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	if (!GetWorld() || StartRealTime < 0 || Duration <= 0) return false;
	const float Elapsed = GetWorld()->GetRealTimeSeconds() - StartRealTime;
	if (Elapsed >= Duration) return false;
	const float Envelope = FMath::Square(1.0f - FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f));
	const float Phase = Elapsed * 2.0f * PI * 28.0f;
	InOutPOV.Location += InOutPOV.Rotation.RotateVector(
		FVector(0, FMath::Cos(Phase), FMath::Sin(Phase * 1.3f) * 0.65f) * Distance * Envelope);
	InOutPOV.Rotation += FRotator(FMath::Sin(Phase), 0, FMath::Cos(Phase * 0.8f) * 0.5f) * Rotation * Envelope;
	return false;
}
