#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CoreChoiceState.h"
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
	 * 미보유 코어는 1단계로 획득하고, 보유 코어는 최대 단계까지 한 단계 강화한다.
	 * 최초 획득은 GE/GA를 부여하며, 강화는 기존 GE/GA의 레벨을 갱신한다. 성공하면 true를 반환한다.
	 */
	bool AcquireCore(UCoreDataAsset* CoreData);

	/** 해당 CoreId의 코어를 보유하고 있는지 확인한다. 강화 가능 여부와는 별개다. */
	UFUNCTION(BlueprintPure, Category = "Core")
	bool HasCore(FName CoreId) const;

	/** 현재 강화 단계를 반환한다. 미보유 코어나 빈 ID는 0이다. UI 등에서 단계 표시에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Core")
	int32 GetCoreLevel(FName CoreId) const;

	/**
	 * 플레이어에게 제시할 코어 선택지를 최대 3개 만든다.
	 * 미보유 또는 강화 가능한 코어를 추첨하며, 기존 선택지가 남아 있으면 새 요청을 거부한다.
	 */
	TArray<UCoreDataAsset*> GenerateCoreChoices(const TArray<UCoreDataAsset*>& CandidatePool);

	/** 창을 닫을 때 진행 중인 선택 기록을 보상 쪽으로 옮겨 보관한다. */
	FCoreChoiceState SuspendCoreChoices();

	/** 보상에 보관한 기록으로 선택을 이어 간다. 획득할 수 없게 된 코어는 제외한다. */
	TArray<UCoreDataAsset*> ResumeCoreChoices(FCoreChoiceState& SavedState);

	/**
	 * 플레이어가 고른 코어를 획득하여 선택 보상을 완료한다.
	 * 현재 선택지에 있는 코어만 획득할 수 있으며, 성공하면 선택 목록을 비우고 true를 반환한다.
	 */
	bool SelectCore(FName CoreId);

	/** 진행 중인 보상의 후보·등장 기록·리롤 사용 횟수를 비운다. 보유 코어는 유지한다. */
	void CancelCoreChoices();

	/** 지정한 카드의 리롤을 아직 사용하지 않았고, 등장하지 않은 대체 코어가 있는지 확인한다. */
	bool CanRerollCoreChoice(int32 ChoiceIndex) const;

	/** 카드 하나를 이번 보상에서 나오지 않은 코어로 바꾼다. 카드별 1회 제한이며 실패하면 횟수를 사용하지 않는다. */
	UCoreDataAsset* RerollCoreChoice(int32 ChoiceIndex);

	/**
	 * 플레이어의 보유 코어와 진행 중인 코어 선택 상태를 초기화한다.
	 * 코어가 부여한 능력과 효과를 제거하며, 기본 능력과 다른 시스템의 효과는 유지한다.
	 */
	void ResetOwnedCores();

	/** 코어 보유 상태가 바뀌었음을 알린다. UI 등에서 변경된 보유 상태를 반영할 때 사용한다. */
	UPROPERTY(BlueprintAssignable, Category = "Core")
	FOnCoreChangedSignature OnCoreChanged;

private:
	/** 카드별 사용 횟수를 확인하고, 최대 단계 코어와 이미 나온 코어를 제외한 재추첨 후보를 모은다. */
	TArray<UCoreDataAsset*> GetRerollCandidates(int32 ChoiceIndex) const;

	/**
	 * 해당 코어를 획득할 수 있는지 사전에 확인한다. 보상 후보를 거를 때도 같은 기준을 사용한다.
	 * 데이터와 ASC, 최대 강화 단계, GE/GA 설정을 검사하며 실제 획득 성공 여부는 AcquireCore에서 확인한다.
	 */
	bool CanAcquireCore(const UCoreDataAsset* CoreData) const;

	/** 기존 GE/GA가 남아 있는지 확인한 뒤 레벨을 올린다. 재부여하지 않으므로 효과와 능력이 중복되지 않는다. */
	bool UpgradeCore(FCoreGrantRecord& Record);

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

	/** 현재 처리 중인 보상 하나의 선택 기록. 창을 닫으면 해당 픽업으로 옮겨 다음 선택까지 보관한다. */
	UPROPERTY(Transient)
	FCoreChoiceState CurrentChoiceState;

	/** 코어 획득 또는 초기화가 진행 중인지 나타낸다. 처리 도중 같은 작업이 다시 시작되는 것을 막는다. */
	bool bIsChangingCoreGrants = false;

	/** 선택한 코어의 획득 알림에서 다른 선택이나 재추첨이 다시 실행되는 것을 막는다. */
	bool bIsSelectingCore = false;
};
