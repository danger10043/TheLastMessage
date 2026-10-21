#pragma once

#include "CoreMinimal.h"
#include "Core/CoreChoiceState.h"
#include "Core/GameCoreTypes.h"
#include "UObject/Object.h"
#include "CoreRewardSession.generated.h"

class UCoreComponent;
class UCoreDataAsset;

/** 보상 하나의 후보, 등장 기록, 카드별 리롤을 관리한다. 실제 지급은 CoreComponent에 요청한다. */
UCLASS(Transient, NotBlueprintable)
class THELASTMESSAGE_API UCoreRewardSession : public UObject
{
	GENERATED_BODY()

public:
	ECoreRewardStartResult Start(UCoreComponent* InCore, const TArray<UCoreDataAsset*>& Pool, ECoreCategory Category);
	TArray<UCoreDataAsset*> GetChoices() const;
	/** 현재 카드와 각 코어의 보유 단계. 선택 화면은 이 값만 표시하고 Pawn을 직접 조회하지 않는다. */
	TArray<FCoreChoiceEntry> GetChoiceEntries() const;
	bool IsActive() const { return bActive && Core.IsValid(); }
	UCoreComponent* GetCoreComponent() const { return Core.Get(); }
	ECoreAcquireResult SelectCore(FName CoreId);
	bool CanRerollCoreChoice(int32 ChoiceIndex) const;
	UCoreDataAsset* RerollCoreChoice(int32 ChoiceIndex);
	void Cancel();

	/** 초기화·소유자 종료·후보 소진으로 선택을 유지할 수 없을 때 알린다. */
	FSimpleMulticastDelegate OnCancelled;

private:
	TArray<UCoreDataAsset*> GetUnshownCandidates() const;
	TArray<UCoreDataAsset*> GetRerollCandidates(int32 ChoiceIndex) const;
	void RepairChoices();
	void Finish(bool bNotifyCancellation);

	UPROPERTY(Transient)
	TWeakObjectPtr<UCoreComponent> Core;

	UPROPERTY(Transient)
	FCoreChoiceState State;

	TSet<FName> RejectedCoreIds;
	FDelegateHandle InvalidationHandle;
	bool bActive = false;
	bool bProcessing = false;
};
