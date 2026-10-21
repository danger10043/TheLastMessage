#include "Character/Boss/BossUIWidget.h"
#include "Components/ProgressBar.h"

void UBossUIWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    SetBossStatus(0.f, false, 0.f);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UBossUIWidget::SetBossStatus(
    float HealthRatio, bool bShowPuzzle, float PuzzleRatio)
{
    if (HealthBar)
    {
        HealthBar->SetPercent(FMath::IsFinite(HealthRatio)
            ? FMath::Clamp(HealthRatio, 0.f, 1.f) : 0.f);
    }

    if (PuzzleRemainTimeBar)
    {
        PuzzleRemainTimeBar->SetPercent(
            bShowPuzzle && FMath::IsFinite(PuzzleRatio)
            ? FMath::Clamp(PuzzleRatio, 0.f, 1.f) : 0.f);
        PuzzleRemainTimeBar->SetVisibility(bShowPuzzle
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }
}