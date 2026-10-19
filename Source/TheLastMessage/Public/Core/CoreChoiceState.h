#pragma once

#include "CoreMinimal.h"
#include "CoreChoiceState.generated.h"

class UCoreDataAsset;

/** 같은 보상의 선택을 이어 가기 위해 후보·등장 기록·카드별 리롤 사용 횟수를 함께 보관한다. */
USTRUCT()
struct THELASTMESSAGE_API FCoreChoiceState
{
	GENERATED_BODY()

	/** 재추첨에 사용할 원본 후보 목록. */
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
