#include "Character/Boss/BossFacingTurn.h"
#include "Character/Boss/BossMonster.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

bool FBossFacingTurn::Start(ABossMonster* Boss, double TargetX,
    const UCurveFloat* SpeedCurve, float Duration, double Now)
{
    Restore();
    UCharacterMovementComponent* Move = IsValid(Boss)
        ? Boss->GetCharacterMovement() : nullptr;
    if (!IsValid(Boss) || !Boss->HasAuthority() || !Move
        || !FMath::IsFinite(TargetX) || !FMath::IsFinite(Duration) || Duration <= 0.f
        || !FMath::IsFinite(Now) || Boss->GetActorLocation().ContainsNaN()
        || Boss->GetActorRotation().ContainsNaN())
    {
        return false;
    }

    Integral.SetNumZeroed(129);
    for (int32 I = 1; I <= 128; ++I)
    {
        const float A = SpeedCurve ? SpeedCurve->GetFloatValue((I - 1) / 128.f) : 1.f;
        const float B = SpeedCurve ? SpeedCurve->GetFloatValue(I / 128.f) : 1.f;
        if (!FMath::IsFinite(A) || !FMath::IsFinite(B))
        {
            return false;
        }
        Integral[I] = Integral[I - 1]
            + (FMath::Max(0.f, A) + FMath::Max(0.f, B)) / 256.f;
    }
    const float Total = Integral.Last();
    if (!FMath::IsFinite(Total) || Total <= UE_SMALL_NUMBER)
    {
        return false;
    }
    for (float& Value : Integral)
    {
        Value /= Total;
    }

    Owner = Boss;
    StartRotation = Boss->GetActorRotation();
    StartTime = Now;
    TurnDuration = Duration;
    const double DeltaX = TargetX - Boss->GetActorLocation().X;
    TargetYaw = DeltaX < -UE_SMALL_NUMBER ? 180.f
        : DeltaX > UE_SMALL_NUMBER ? 0.f
        : (FMath::Cos(FMath::DegreesToRadians(StartRotation.Yaw)) >= 0.f ? 0.f : 180.f);
    bSavedControllerYaw = Boss->bUseControllerRotationYaw;
    bSavedOrientToMovement = Move->bOrientRotationToMovement;
    bSavedDesiredRotation = Move->bUseControllerDesiredRotation;
    bOwnsRotation = true;
    bFinished = false;
    Boss->bUseControllerRotationYaw = false;
    Move->bOrientRotationToMovement = false;
    Move->bUseControllerDesiredRotation = false;
    return Update(Now);
}

bool FBossFacingTurn::Update(double Now)
{
    ABossMonster* Boss = Owner.Get();
    if (!bOwnsRotation || !IsValid(Boss) || !Boss->HasAuthority()
        || !FMath::IsFinite(Now))
    {
        return false;
    }
    if (bFinished)
    {
        return true;
    }

    const float T = FMath::Clamp(
        static_cast<float>((Now - StartTime) / TurnDuration), 0.f, 1.f);
    const float Sample = T * 128.f;
    const int32 I = FMath::Min(FMath::FloorToInt(Sample), 127);
    const float Alpha = FMath::Lerp(Integral[I], Integral[I + 1], Sample - I);
    FRotator Rotation = StartRotation;
    Rotation.Yaw += FMath::FindDeltaAngleDegrees(
        static_cast<float>(StartRotation.Yaw), TargetYaw) * Alpha;
    Boss->SetActorRotation(Rotation);
    if (T >= 1.f)
    {
        Finish();
    }
    return true;
}

void FBossFacingTurn::Finish()
{
    if (ABossMonster* Boss = Owner.Get(); bOwnsRotation && IsValid(Boss))
    {
        FRotator Rotation = StartRotation;
        Rotation.Yaw = TargetYaw;
        Boss->SetActorRotation(Rotation);
        bFinished = true;
    }
}

void FBossFacingTurn::Restore()
{
    if (ABossMonster* Boss = Owner.Get(); bOwnsRotation && IsValid(Boss))
    {
        if (AController* Controller = Boss->GetController())
        {
            Controller->SetControlRotation(Boss->GetActorRotation());
        }
        Boss->bUseControllerRotationYaw = bSavedControllerYaw;
        if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
        {
            Move->bOrientRotationToMovement = bSavedOrientToMovement;
            Move->bUseControllerDesiredRotation = bSavedDesiredRotation;
        }
    }
    bOwnsRotation = false;
    bFinished = false;
    Owner.Reset();
    Integral.Reset();
}