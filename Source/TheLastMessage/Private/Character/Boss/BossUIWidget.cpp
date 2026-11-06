#include "Character/Boss/BossUIWidget.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    constexpr float TrailHoldSeconds = 0.5f;
    constexpr float TrailFadeSeconds = 0.30f;
    constexpr float FlashSeconds = 0.16f;
    const FName FlashParameterName(TEXT("Flash"));
}

void UBossUIWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    NormalFillBrush = HealthBar->GetWidgetStyle().FillImage;
    NormalFillColor = HealthBar->GetFillColorAndOpacity();
    FrameMaterial = HealthBarFrameImage->GetDynamicMaterial();

    SetBossStatus(0.f, false, 0.f, false);
    bHasHealthSample = false;
    SetVisibility(ESlateVisibility::Collapsed);
}

void UBossUIWidget::SetBossStatus(
    float HealthRatio, bool bShowPuzzle, float PuzzleRatio, bool bShieldActive)
{
    const float NewHealthRatio = FMath::IsFinite(HealthRatio)
        ? FMath::Clamp(HealthRatio, 0.f, 1.f) : 0.f;

    if (!bHasHealthSample || NewHealthRatio > CurrentHealthRatio)
    {
        DamageTrailBar->SetPercent(NewHealthRatio);
        TrailHoldRemaining = 0.f;
        TrailFadeElapsed = TrailFadeSeconds;
    }
    else if (NewHealthRatio < CurrentHealthRatio)
    {
        TrailStartRatio = DamageTrailBar->GetPercent();
        TrailHoldRemaining = TrailHoldSeconds;
        TrailFadeElapsed = 0.f;

        FlashRemaining = FlashSeconds;
        if (FrameMaterial)
        {
            FrameMaterial->SetScalarParameterValue(FlashParameterName, 0.8f);
        }
    }

    bHasHealthSample = true;
    CurrentHealthRatio = NewHealthRatio;
    HealthBar->SetPercent(CurrentHealthRatio);
    UpdateShieldAppearance(bShieldActive);

    const float RemainingRatio = bShowPuzzle && FMath::IsFinite(PuzzleRatio)
        ? FMath::Clamp(PuzzleRatio, 0.f, 1.f) : 0.f;

    PuzzleRemainTimeBar->SetPercent(RemainingRatio);

    if (UMaterialInstanceDynamic* GlowMaterial = PuzzleTimerGlowImage->GetDynamicMaterial())
    {
        static const FName RemainingRatioParameter(TEXT("RemainingRatio"));
        GlowMaterial->SetScalarParameterValue(RemainingRatioParameter, RemainingRatio);
    }

    PuzzleTimerGlowImage->SetVisibility(RemainingRatio > 0.f
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);

    PuzzleTimerGroup->SetVisibility(bShowPuzzle
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);
}

void UBossUIWidget::NativeTick(
    const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (TrailFadeElapsed < TrailFadeSeconds)
    {
        const float HoldTimeUsed = FMath::Min(TrailHoldRemaining, InDeltaTime);
        TrailHoldRemaining -= HoldTimeUsed;

        const float FadeDeltaTime = InDeltaTime - HoldTimeUsed;
        TrailFadeElapsed = FMath::Min(
            TrailFadeElapsed + FadeDeltaTime, TrailFadeSeconds);

        const float Alpha = TrailFadeElapsed / TrailFadeSeconds;
        DamageTrailBar->SetPercent(FMath::Lerp(
            TrailStartRatio, CurrentHealthRatio, Alpha));
    }

    if (FlashRemaining > 0.f)
    {
        FlashRemaining = FMath::Max(FlashRemaining - InDeltaTime, 0.f);
        if (FrameMaterial)
        {
            FrameMaterial->SetScalarParameterValue(
                FlashParameterName, 0.8f * FlashRemaining / FlashSeconds);
        }
    }
}

void UBossUIWidget::UpdateShieldAppearance(bool bShieldActive)
{
    const bool bUseShieldFill = bShieldActive && ShieldFillMaterial != nullptr;
    if (bShowingShieldFill == bUseShieldFill)
    {
        return;
    }

    FProgressBarStyle Style = HealthBar->GetWidgetStyle();
    Style.FillImage = NormalFillBrush;
    if (bUseShieldFill)
    {
        Style.FillImage.SetResourceObject(ShieldFillMaterial);
        Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
    }

    HealthBar->SetWidgetStyle(Style);
    HealthBar->SetFillColorAndOpacity(
        bUseShieldFill ? FLinearColor::White : NormalFillColor);
    bShowingShieldFill = bUseShieldFill;
}