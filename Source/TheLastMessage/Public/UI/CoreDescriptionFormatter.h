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
}
