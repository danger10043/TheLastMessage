#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Character/Enemy/DataAsset/EnemyTypes.h"
#include "EnemyAIController.generated.h"

class AEnemyBase;
class UBehaviorTree;
class UBlackboardData;
class UAISenseConfig_Sight;

namespace EnemyBB
{
	inline const FName TargetActor(TEXT("TargetActor"));
	inline const FName EnemyState(TEXT("EnemyState"));
	inline const FName TargetDistance(TEXT("TargetDistance"));
	inline const FName HomeLocation(TEXT("HomeLocation"));
	inline const FName bCanAttack(TEXT("bCanAttack"));
	inline const FName PatrolLocation(TEXT("PatrolLocation")); // 정찰 지점 (명세서 외 추가 키)
	inline const FName LastKnownLocation(TEXT("LastKnownLocation")); // 플레이어를 놓친 위치 (명세서 외 추가 키)
}

UCLASS()
class THELASTMESSAGE_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	void StartEnemyAI();
	void StopEnemyAI();

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void UpdateTargetActor(AActor* NewTarget);

	UFUNCTION()
	void UpdateEnemyState(EEnemyState NewState);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBlackboardData> BlackboardAsset;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<AEnemyBase> ControlledEnemy;

private:
	void UpdateBlackboardTargetInfo();

	FTimerHandle BlackboardUpdateTimer;
};
