#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Boss/BossTypes.h"
#include "BossPhaseComponent.generated.h"

UCLASS()
class THELASTMESSAGE_API UBossPhaseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void CheckPhaseThreshold(float HealthPercent);

    void RequestPhaseTransition(EBossPhase NextPhase);

    void CommitPhaseTransition();

    void CancelPhaseTransition();

    EBossPhase GetCurrentPhase();

    bool TryGetPendingPhase(EBossPhase& OutPhase) const;

    float GetDamageHealthFloorRatio() const;

    bool IsPhaseUnlocked(EBossPhase Phase);

    void ApplyPhaseSettings(EBossPhase NewPhase);

private:
    UPROPERTY(VisibleInstanceOnly, Category = "Boss|Phase")
    EBossPhase CurrentPhase = EBossPhase::Phase1;

    UPROPERTY()
    EBossPhase PendingPhase = EBossPhase::Phase1;

    UPROPERTY(EditDefaultsOnly)
    float Phase2Threshold = 0.7f;

    UPROPERTY(EditDefaultsOnly)
    float Phase3Threshold = 0.3f;

    UPROPERTY()
    bool bPhase2Unlocked = false;

    UPROPERTY()
    bool bPhase3Unlocked = false;

    UPROPERTY()
    bool bIsTransitionPending = false;
};