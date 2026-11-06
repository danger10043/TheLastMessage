#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossUIWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UOverlay;
class UProgressBar;

UCLASS(Abstract, Blueprintable)
class THELASTMESSAGE_API UBossUIWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetBossStatus(
        float HealthRatio, bool bShowPuzzle, float PuzzleRatio, bool bShieldActive);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> DamageTrailBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> HealthBarFrameImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PuzzleRemainTimeBar;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UOverlay> PuzzleTimerGroup;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> PuzzleTimerGlowImage;

    UPROPERTY(EditDefaultsOnly, Category = "Boss UI")
    TObjectPtr<UMaterialInterface> ShieldFillMaterial;

private:
    void UpdateShieldAppearance(bool bShieldActive);

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> FrameMaterial;

    UPROPERTY(Transient)
    FSlateBrush NormalFillBrush;

    FLinearColor NormalFillColor = FLinearColor::White;
    float CurrentHealthRatio = 0.f;
    float TrailStartRatio = 0.f;
    float TrailHoldRemaining = 0.f;
    float TrailFadeElapsed = 0.f;
    float FlashRemaining = 0.f;
    bool bHasHealthSample = false;
    bool bShowingShieldFill = false;
};