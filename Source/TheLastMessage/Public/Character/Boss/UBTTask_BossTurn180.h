#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Character/Boss/BossTypes.h"
#include "UBTTask_BossTurn180.generated.h"

class ABossMonster;
class AAIController;
class UAnimInstance;
class UAnimMontage;
class UBlackboardComponent;

UCLASS()
class THELASTMESSAGE_API UBTTask_BossTurn180 : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_BossTurn180();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

    UPROPERTY(EditAnywhere, Category = "Boss")
    TObjectPtr<UAnimMontage> TurnMontage;

    UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.1"))
    float PlayRate = 1.f;

private:
    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    UFUNCTION()
    void HandleBossStateChanged(EBossState NewState);
    void Cleanup(bool bStopMontage);

    TWeakObjectPtr<ABossMonster> CachedBoss;
    TWeakObjectPtr<AAIController> CachedAI;
    TWeakObjectPtr<UAnimInstance> CachedAnim;
    TWeakObjectPtr<UBlackboardComponent> CachedBB;
    int32 ActiveMontageID = INDEX_NONE;
    bool bFinished = false;
    bool bWasInterrupted = false;
    bool bSavedRotation = false;
    bool bSavedUseControllerYaw = false;
    bool bSavedOrientToMovement = false;
    bool bSavedDesiredRotation = false;
};