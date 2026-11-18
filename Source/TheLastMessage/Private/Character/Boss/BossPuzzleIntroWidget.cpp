#include "Character/Boss/BossPuzzleIntroWidget.h"
#include "Animation/WidgetAnimation.h"

void UBossPuzzleIntroWidget::PlayIntro()
{
    if (bIntroPlaying)
    {
        return;
    }
    if (!IsValid(StartAnim.Get()) || !IsValid(EndAnim.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("레이저 퍼즐 UI의 StartAnim과 EndAnim을 확인하세요."));
        RemoveFromParent();
        return;
    }

    bIntroPlaying = true;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    PlayAnimation(StartAnim.Get(), 0.f, 1, EUMGSequencePlayMode::Forward, 1.f, false);
}

void UBossPuzzleIntroWidget::OnAnimationFinished_Implementation(
    const UWidgetAnimation* Animation)
{
    Super::OnAnimationFinished_Implementation(Animation);

    if (!bIntroPlaying)
    {
        return;
    }
    if (Animation == StartAnim.Get())
    {
        PlayAnimation(EndAnim.Get(), 0.f, 1, EUMGSequencePlayMode::Forward, 1.f, false);
    }
    else if (Animation == EndAnim.Get())
    {
        bIntroPlaying = false;
        RemoveFromParent();
    }
}

void UBossPuzzleIntroWidget::NativeDestruct()
{
    bIntroPlaying = false;
    StopAllAnimations();
    Super::NativeDestruct();
}