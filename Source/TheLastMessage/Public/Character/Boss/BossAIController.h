#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Character/Boss/BossTypes.h"
#include "BossAIController.generated.h"

class UBehaviorTree;
class UBlackboardData;
class ABossMonster;

UCLASS()
class THELASTMESSAGE_API ABossAIController : public AAIController
{
    GENERATED_BODY()

public:
    virtual void OnPossess(APawn* InPawn) override;
    void StartBossAI();
    void StopBossAI();
    void PauseBossAI();
    void ResumeBossAI();
    void UpdateTargetActor(AActor* NewTarget);
    void UpdateBossPhase(EBossPhase NewPhase);
    void UpdateBossState(EBossState NewState);

protected:
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UBlackboardData> BlackboardAsset;

    UPROPERTY()
    TObjectPtr<ABossMonster> ControlledBoss = nullptr;
};