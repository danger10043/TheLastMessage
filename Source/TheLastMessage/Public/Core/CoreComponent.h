#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GameCoreTypes.h"
#include "Core/CoreGrantRecord.h"
#include "CoreComponent.generated.h"

class UAbilitySystemComponent;
class UCoreDataAsset;
class UCoreRewardSession;

/** 코어 단계 변화 알림. 신규 획득은 OldLevel 0, 초기화로 제거되면 NewLevel 0이다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCoreChangedSignature, FName, CoreId, int32, OldLevel, int32, NewLevel);

/**
 * 플레이어의 코어 획득과 보유 상태를 관리하는 컴포넌트다.
 * 획득한 코어의 능력과 효과를 플레이어 ASC에 부여하고, 초기화할 때 제거한다.
 * 능력과 효과를 부여할 대상을 알 수 있도록 플레이어의 ASC를 전달받아 보관한다.
 */
UCLASS(ClassGroup = (Core), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UCoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCoreComponent();

	/**
	 * 코어의 능력과 효과를 받을 플레이어 ASC를 연결한다. 플레이어의 GAS 초기화가 끝난 뒤 호출한다.
	 * 부여 기록이 이 ASC의 핸들에 묶이므로 Blueprint에서 바꿔 끼우지 않도록 C++에서만 호출한다.
	 */
	void Initialize(UAbilitySystemComponent* InASC);

	/**
	 * 미보유 코어는 1단계로 획득한다. 재획득은 Custom의 최대 단계 또는 Infinite 설정에 따라 한 단계 강화한다.
	 * 최초 획득은 GE/GA를 부여하며, 강화는 기존 GE/GA의 레벨을 갱신한다.
	 * 성공하면 Acquired 또는 Upgraded를, 실패하면 원인을 반환한다. IsCoreAcquireSuccess로 성공 여부를 확인한다.
	 * 변경 알림이 모두 끝나기 전의 추가 획득 요청은 Busy로 거부하고, 초기화 요청은 처리하지 않는다.
	 */
	ECoreAcquireResult AcquireCore(UCoreDataAsset* CoreData);

	/** 해당 CoreId의 코어를 보유하고 있는지 확인한다. 강화 가능 여부와는 별개다. */
	UFUNCTION(BlueprintPure, Category = "Core")
	bool HasCore(FName CoreId) const;

	/** 현재 강화 단계를 반환한다. 미보유 코어나 빈 ID는 0이다. UI 등에서 단계 표시에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Core")
	int32 GetCoreLevel(FName CoreId) const;

	/** 현재 보유 상태와 부여 설정을 검사해 예상 획득 결과를 반환한다. 실제 GE/GA 적용은 하지 않는다. */
	ECoreAcquireResult CheckAcquire(const UCoreDataAsset* CoreData) const;

	/**
	 * 플레이어의 보유 코어와 진행 중인 코어 선택 상태를 초기화한다.
	 * 코어가 부여한 능력과 효과를 제거하며, 기본 능력과 다른 시스템의 효과는 유지한다.
	 * 제거 알림이 모두 끝나기 전의 재획득 요청은 Busy로 거부한다. 초기화가 반환된 뒤에는 다시 획득할 수 있다.
	 */
	void ResetOwnedCores();

	/** 코어의 단계가 바뀌었음을 알린다. 획득·강화 연출이나 보유 목록 UI 갱신에 사용한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core")
	FOnCoreChangedSignature OnCoreChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UCoreRewardSession;

	/** 한 플레이어의 보상 선택을 한 요청자가 점유하도록 관리한다. 추첨 데이터는 보관하지 않는다. */
	bool TryReserveReward(UObject* Requester);
	void ReleaseReward(const UObject* Requester);
	FSimpleMulticastDelegate OnRewardInvalidated;

	/** 획득 검사를 통과한 기존 GE/GA의 레벨을 갱신한다. 효과와 능력을 새로 부여하지 않는다. */
	void UpgradeCore(FCoreGrantRecord& Record);

	/**
	 * 특정 코어가 플레이어에게 부여한 능력과 효과를 제거한다.
	 * 기록된 핸들로 제거 대상을 찾아, 획득 실패 시 이미 부여한 내용을 되돌리거나 보유 코어를 초기화한다.
	 */
	void RemoveCoreGrants(const FCoreGrantRecord& Record);

	/** 코어의 능력과 효과를 부여할 플레이어 ASC. 사용 시 유효성을 확인할 수 있도록 약한 참조로 보관한다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * 보유 코어를 조회하고, 각 코어가 부여한 능력과 효과를 제거하기 위한 기록이다.
	 * 코어별 CoreId, 현재 강화 단계, 부여한 GE/GA 핸들을 보관한다.
	 * 강화는 기존 핸들의 레벨을 갱신하고, 초기화는 이 핸들로 능력과 효과를 제거한다.
	 * 실행 중 기록이며 저장·복원 기능은 별도로 구현해야 한다. 실행 중 핸들을 저장 데이터로 사용하지 않는다.
	 */
	UPROPERTY(Transient)
	TArray<FCoreGrantRecord> AppliedCoreHandles;

	/** 진행 중인 보상 요청자의 식별자. 해당 보상에서만 점유를 해제할 수 있다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> ActiveRewardOwner;

	/** 코어 획득·초기화와 해당 알림이 진행 중인지 나타낸다. 처리 도중 같은 작업이 다시 시작되는 것을 막는다. */
	bool bIsChangingCoreGrants = false;

};
