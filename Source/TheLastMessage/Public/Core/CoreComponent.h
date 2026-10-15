#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CoreGrantRecord.h"
#include "CoreComponent.generated.h"

class UAbilitySystemComponent;
class UCoreDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCoreChangedSignature);

/**
 * 플레이어의 코어 획득과 보유 상태, 선택 보상을 관리하는 컴포넌트다.
 * 획득한 코어의 능력과 효과를 플레이어 ASC에 부여하고, 초기화할 때 제거한다.
 * 능력과 효과를 부여할 대상을 알 수 있도록 플레이어의 ASC를 전달받아 보관한다.
 */
UCLASS(ClassGroup = (Core), meta = (BlueprintSpawnableComponent))
class THELASTMESSAGE_API UCoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCoreComponent();

	/** 코어의 능력과 효과를 받을 플레이어 ASC를 연결한다. 플레이어의 GAS 초기화가 끝난 뒤 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Core")
	void Initialize(UAbilitySystemComponent* InASC);

	/**
	 * 코어를 획득하여 플레이어가 그 능력과 효과를 사용할 수 있게 한다.
	 * GE/GA 부여와 보유 기록 등록까지 모두 성공하면 true를 반환한다.
	 */
	bool AcquireCore(UCoreDataAsset* CoreData);

	/** 해당 CoreId의 코어를 보유하고 있는지 확인한다. 중복 획득과 보상 후보 제외 판정에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Core")
	bool HasCore(FName CoreId) const;

	/**
	 * 플레이어에게 제시할 코어 선택지를 최대 3개 만든다.
	 * 유효한 미보유 코어를 추첨하며, 기존 선택지가 남아 있으면 새 요청을 거부한다.
	 */
	TArray<UCoreDataAsset*> GenerateCoreChoices(const TArray<UCoreDataAsset*>& CandidatePool);

	/**
	 * 플레이어가 고른 코어를 획득하여 선택 보상을 완료한다.
	 * 현재 선택지에 있는 코어만 획득할 수 있으며, 성공하면 선택 목록을 비우고 true를 반환한다.
	 */
	bool SelectCore(FName CoreId);

	/** 현재 코어 선택지를 다시 뽑는다. 처음 전달받은 전체 후보에서 획득 가능한 코어를 다시 추첨한다. */
	TArray<UCoreDataAsset*> RerollCoreChoices();

	/**
	 * 플레이어의 보유 코어와 진행 중인 코어 선택 상태를 초기화한다.
	 * 코어가 부여한 능력과 효과를 제거하며, 기본 능력과 다른 시스템의 효과는 유지한다.
	 */
	void ResetOwnedCores();

	/** 코어 보유 상태가 바뀌었음을 알린다. UI 등에서 변경된 보유 상태를 반영할 때 사용한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core")
	FOnCoreChangedSignature OnCoreChanged;

private:
	/**
	 * 해당 코어를 획득할 수 있는지 사전에 확인한다. 보상 후보를 거를 때도 같은 기준을 사용한다.
	 * 데이터와 ASC, 보유 여부, GE/GA 설정을 검사하며 실제 획득 성공 여부는 AcquireCore에서 확인한다.
	 */
	bool CanAcquireCore(const UCoreDataAsset* CoreData) const;

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
	 * 획득이 완료된 코어별로 CoreId와 부여한 GE/GA 핸들을 보관한다.
	 * HasCore는 이 기록의 CoreId를 조회하고, 코어 초기화는 기록된 핸들로 능력과 효과를 제거한다.
	 * 불러오기 시 보유 CoreId를 기준으로 능력과 효과를 다시 부여하여 이 실행의 기록을 구성한다.
	 */
	UPROPERTY(Transient)
	TArray<FCoreGrantRecord> AppliedCoreHandles;

	/** 재추첨에 사용할 전체 후보 목록. 선택 보상을 열 때 전달받은 원본 후보를 보관한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCoreDataAsset>> CurrentCandidatePool;

	/** 현재 제시한 선택지. 선택 요청이 실제로 제시된 코어인지 검사할 때 사용한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCoreDataAsset>> CurrentChoices;

	/** 코어 획득 또는 초기화가 진행 중인지 나타낸다. 처리 도중 같은 작업이 다시 시작되는 것을 막는다. */
	bool bIsChangingCoreGrants = false;

	/** 선택한 코어의 획득 알림에서 다른 선택이나 재추첨이 다시 실행되는 것을 막는다. */
	bool bIsSelectingCore = false;
};
