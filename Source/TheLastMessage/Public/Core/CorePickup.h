#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "CorePickup.generated.h"

class UCoreComponent;
class UCoreDataAsset;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

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
class THELASTMESSAGE_API ACorePickup : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ACorePickup();

	/** 요청한 액터가 이 고정 코어 보상과 상호작용할 수 있는지 확인한다. */
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	/** 상호작용한 액터의 코어 컴포넌트에 고정 코어 획득을 요청한다. */
	virtual void Interact_Implementation(AActor* Interactor) override;

	/** 지정된 코어의 획득을 요청한다. 성공하면 보상을 소비하고 true를 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "Core|Reward")
	bool TryAcquireFixedCore(UCoreComponent* InCoreComponent);

protected:
	/** 플레이어가 코어를 획득할 수 있는 범위를 감지한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|Components")
	TObjectPtr<USphereComponent> PickupCollision = nullptr;

	/** 코어 보상의 외형을 표시하는 메시 컴포넌트다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Core|Components")
	TObjectPtr<UStaticMeshComponent> CoreMesh = nullptr;

	/** 지정 코어 지급과 코어 선택 보상 중 사용할 방식을 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward")
	ECoreRewardMode RewardMode = ECoreRewardMode::Fixed;

	/** 고정 보상으로 지급할 코어의 정의 데이터다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Fixed", EditConditionHides))
	TObjectPtr<UCoreDataAsset> CoreData = nullptr;

	/** 선택형 보상에서 플레이어에게 제시할 후보를 추첨하는 데 사용하는 전체 코어 목록이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Reward",
		meta = (EditCondition = "RewardMode == ECoreRewardMode::Choice", EditConditionHides))
	TArray<TObjectPtr<UCoreDataAsset>> CandidatePool;

	/** 보상 지급이 완료되었는지 나타낸다. 이미 소비한 보상의 재지급을 막는다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Core|Reward")
	bool bConsumed = false;

private:
	/** 획득 범위에 들어온 플레이어에게 고정 코어의 획득을 요청한다. */
	UFUNCTION()
	void OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 현재 코어 보상을 지급하는 중인지 나타낸다. 지급 중에는 추가 획득 요청을 막는다. */
	bool bIsGrantingReward = false;
};
