#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Character/Boss/BossTypes.h"
#include "GameplayAbilitySpec.h"
#include "AITypes.h"
#include "BTTask_BossChaseOrGunShot.generated.h"

class APlayerBase;
class UGA_BossGunShot;
class UAbilitySystemComponent;
class UPathFollowingComponent;
struct FPathFollowingResult;
struct FAbilityEndedData;

UCLASS()
class THELASTMESSAGE_API UBTTask_BossChaseOrGunShot : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_BossChaseOrGunShot();
    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

    UPROPERTY(EditAnywhere, Category = "Boss|Move", meta = (ClampMin = "0.0"))
    float AcceptanceRadius = 100.f;
    UPROPERTY(EditAnywhere, Category = "Boss|GunShot")
    TSubclassOf<UGA_BossGunShot> GunShotAbilityClass;

private:
    void HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
    void HandleAbilityEnded(const FAbilityEndedData& Data);
    void Cleanup();

    TWeakObjectPtr<APlayerBase> Target;
    TWeakObjectPtr<UPathFollowingComponent> PathFollowing;
    TWeakObjectPtr<UAbilitySystemComponent> ActiveASC;
    FAIRequestID MoveRequestID = FAIRequestID::InvalidRequest;
    FGameplayAbilitySpecHandle ActiveHandle;
    FDelegateHandle MoveDelegate;
    FDelegateHandle AbilityDelegate;
    EBossPhase LockedPhase = EBossPhase::Phase1;
    bool bMoving = false;
    bool bMoveFinished = false;
    bool bMoveFailed = false;
    bool bMoveSucceeded = false;
    bool bAbilityEnded = false;
    bool bAbilityCancelled = false;
};