#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "Character/Boss/BossTypes.h"
#include "UBTService_BossCombatRange.generated.h"

UCLASS()
class THELASTMESSAGE_API UBTService_BossCombatRange : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_BossCombatRange();

protected:
    virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Boss")
    EBossAttackType AttackType = EBossAttackType::Melee;

    UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
    float CombatRadius = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "90.0", ClampMax = "180.0"))
    float TurnThresholdDegrees = 160.f;

private:
    void UpdateValues(UBehaviorTreeComponent& OwnerComp);
};