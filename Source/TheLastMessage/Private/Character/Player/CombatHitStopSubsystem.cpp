#include "Character/Player/CombatHitStopSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

bool UCombatHitStopSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCombatHitStopSubsystem::RequestHitStop(float Duration, float TimeScale)
{
	if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone || !FMath::IsFinite(Duration)
		|| !FMath::IsFinite(TimeScale) || Duration <= 0 || TimeScale <= 0 || TimeScale >= 1) return;
	AWorldSettings* Settings = GetWorld()->GetWorldSettings();
	if (!Settings) return;
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (bActive && Now >= EndRealTime) RestoreTime();
	if (!bActive)
	{
		PreviousTimeScale = Settings->TimeDilation;
		BurstStartTime = Now;
		EndRealTime = Now;
		AppliedTimeScale = PreviousTimeScale;
		bActive = true;
	}
	// Several simultaneous contacts share a bounded stop instead of multiplying slowdowns.
	EndRealTime = FMath::Min(BurstStartTime + 0.12, FMath::Max(EndRealTime, Now + FMath::Min(Duration, 0.12f)));
	AppliedTimeScale = Settings->SetTimeDilation(FMath::Min(AppliedTimeScale,
		PreviousTimeScale * FMath::Clamp(TimeScale, 0.001f, 1.0f)));
}

void UCombatHitStopSubsystem::RestoreTime()
{
	if (!bActive) return;
	if (AWorldSettings* Settings = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr)
	{
		// Do not overwrite a newer slow-motion change made by another system.
		if (FMath::IsNearlyEqual(Settings->TimeDilation, AppliedTimeScale)) Settings->SetTimeDilation(PreviousTimeScale);
	}
	bActive = false;
}

void UCombatHitStopSubsystem::Tick(float DeltaTime)
{
	if (bActive && GetWorld()->GetRealTimeSeconds() >= EndRealTime) RestoreTime();
}

TStatId UCombatHitStopSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatHitStopSubsystem, STATGROUP_Tickables);
}

void UCombatHitStopSubsystem::Deinitialize()
{
	RestoreTime();
	Super::Deinitialize();
}
