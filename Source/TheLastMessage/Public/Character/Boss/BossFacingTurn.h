#pragma once

#include "CoreMinimal.h"

class ABossMonster;
class UCurveFloat;

struct THELASTMESSAGE_API FBossFacingTurn
{
    bool Start(ABossMonster* Boss, double TargetX,
        const UCurveFloat* SpeedCurve, float Duration, double Now);
    bool Update(double Now);
    void Finish();
    void Restore();

private:
    TWeakObjectPtr<ABossMonster> Owner;
    TArray<float> Integral;
    FRotator StartRotation = FRotator::ZeroRotator;
    double StartTime = 0.0;
    float TurnDuration = 0.f;
    float TargetYaw = 0.f;
    bool bOwnsRotation = false;
    bool bFinished = false;
    bool bSavedControllerYaw = false;
    bool bSavedOrientToMovement = false;
    bool bSavedDesiredRotation = false;
};