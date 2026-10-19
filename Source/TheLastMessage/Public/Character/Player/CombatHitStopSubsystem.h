#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatHitStopSubsystem.generated.h"

/** One owner per world prevents overlapping hits from restoring the wrong time scale. */
UCLASS()
class THELASTMESSAGE_API UCombatHitStopSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	void RequestHitStop(float Duration, float TimeScale);
	void RestoreTime();
	bool IsHitStopActive() const { return bActive; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	bool bActive = false;
	float PreviousTimeScale = 1.0f;
	float AppliedTimeScale = 1.0f;
	double BurstStartTime = 0;
	double EndRealTime = 0;
};
