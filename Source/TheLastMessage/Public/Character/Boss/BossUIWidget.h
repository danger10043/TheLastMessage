#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossUIWidget.generated.h"

class UProgressBar;

UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UBossUIWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetBossStatus(float HealthRatio, bool bShowPuzzle, float PuzzleRatio);

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PuzzleRemainTimeBar;
};