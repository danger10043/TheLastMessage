#pragma once

#include "CoreMinimal.h"

class UCoreDataAsset;

/** 표시용 텍스트만 만든다. 보유 단계나 능력치, GE와 DA를 변경하지 않는다. */
namespace CoreDescriptionFormatter
{
	THELASTMESSAGE_API FText MakeLevelText(const UCoreDataAsset& Data, int32 CurrentLevel);
	THELASTMESSAGE_API FText MakeDescription(const UCoreDataAsset& Data, int32 CurrentLevel);
	/** 지정 단계의 효과만 표시한다. 보유 목록의 현재 효과와 다음 단계 설명에 사용한다. */
	THELASTMESSAGE_API FText MakeDescriptionAtLevel(const UCoreDataAsset& Data, int32 Level);
	/** 보유한 단계가 더 강화할 수 없는 마지막 단계인지 판단한다. 무한 강화 코어는 항상 false다. */
	THELASTMESSAGE_API bool IsMaxLevel(const UCoreDataAsset& Data, int32 Level);
	/** 보유 화면처럼 다음 단계 없이 지금 단계만 보여 줄 때의 문구. 예: "Lv.2 / 5", "Lv.5 / 5 · MAX", "Lv.3" */
	THELASTMESSAGE_API FText MakeCurrentLevelText(const UCoreDataAsset& Data, int32 Level);
}
