#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayAbilitySpec.h"
#include "BTTask_BossLightningAttack.generated.h"

class UAbilitySystemComponent;
class UGA_BossLightningAttack;
struct FAbilityEndedData;

UCLASS()
class THELASTMESSAGE_API UBTTask_BossLightningAttack : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_BossLightningAttack();

    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

    UPROPERTY(EditAnywhere, Category = "Boss")
    TSubclassOf<UGA_BossLightningAttack> LightningAbilityClass;

private:
    void HandleAbilityEnded(const FAbilityEndedData& Data);
    void Cleanup(bool bCancelAbility);

    TWeakObjectPtr<UAbilitySystemComponent> ActiveASC;
    FGameplayAbilitySpecHandle ActiveHandle;
    FDelegateHandle EndedDelegateHandle;
    bool bAbilityEnded = false;
    bool bAbilityCancelled = false;
};