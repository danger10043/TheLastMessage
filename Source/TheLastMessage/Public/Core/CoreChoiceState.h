#pragma once

#include "CoreMinimal.h"
#include "CoreChoiceState.generated.h"

class UCoreDataAsset;

/** 선택 화면이 열려 있는 동안 후보·등장 기록·카드별 리롤 사용 횟수를 함께 관리한다. */
USTRUCT()
struct THELASTMESSAGE_API FCoreChoiceState
{
	GENERATED_BODY()

	/** 보상 분류로 걸러 둔 후보 목록. 같은 선택 화면의 리롤에서도 이 풀을 사용한다. */
	UPROPERTY()
	TArray<TObjectPtr<UCoreDataAsset>> CandidatePool;

	/** 마지막으로 제시한 코어 목록. 배열 순서가 화면의 카드 순서다. */
	UPROPERTY()
	TArray<TObjectPtr<UCoreDataAsset>> Choices;

	/** 이번 보상에서 이미 등장한 코어. 리롤로 사라진 코어도 기록을 유지한다. */
	UPROPERTY()
	TSet<FName> ShownCoreIds;

	/** Choices와 같은 순서로 기록한 리롤 사용 횟수. 카드마다 한 번만 사용할 수 있다. */
	UPROPERTY()
	TArray<int32> RerollCounts;
};
