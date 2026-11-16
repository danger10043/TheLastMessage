#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossPuzzleIntroWidget.generated.h"

class UWidgetAnimation;

UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UBossPuzzleIntroWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void PlayIntro();

protected:
    virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
    virtual void NativeDestruct() override;

    UPROPERTY(Transient, meta = (BindWidgetAnim))
    TObjectPtr<UWidgetAnimation> StartAnim;

    UPROPERTY(Transient, meta = (BindWidgetAnim))
    TObjectPtr<UWidgetAnimation> EndAnim;

private:
    bool bIntroPlaying = false;
};