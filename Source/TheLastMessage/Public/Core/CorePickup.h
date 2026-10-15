#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CorePickup.generated.h"

class UCoreComponent;
class UCoreDataAsset;

/** 코어 보상을 지정된 코어로 지급할지, 선택지로 제공할지 구분한다. */
UENUM(BlueprintType)


enum class ECoreRewardMode : uint8
{
	Fixed,		 /** 지정된 코어 하나를 획득한다. */
	Choice		/** 제시된 코어 중 하나를 선택한다. */
};

/**
 * 맵에 배치하여 플레이어에게 코어 보상을 제공하는 액터다.
 * 실제 코어의 능력과 효과 부여는 전달받은 UCoreComponent에 맡긴다.
 */
UCLASS(Blueprintable)
class THELASTMESSAGE_API ACorePickup : public AActor
{
	GENERATED_BODY()

public:
	ACorePickup();

	/** 지정된 코어의 획득을 요청한다. 성공하면 보상을 소비하고 true를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|Reward")
	bool TryAcquireFixedCore(UCoreComponent* InCoreComponent);

protected:
	/** 지정 코어 지급과 코어 선택 보상 중 사용할 방식을 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward")
	ECoreRewardMode RewardMode = ECoreRewardMode::Fixed;

	/** 고정 보상으로 지급할 코어의 정의 데이터다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Fixed", EditConditionHides))
	TObjectPtr<UCoreDataAsset> CoreData = nullptr;

	/** 보상 지급이 완료되었는지 나타낸다. 이미 소비한 보상의 재지급을 막는다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Core|Reward")
	bool bConsumed = false;

private:
	/** 획득 알림에서 같은 보상의 지급 처리가 다시 시작되는 것을 막는다. */
	bool bIsGrantingReward = false;
};
