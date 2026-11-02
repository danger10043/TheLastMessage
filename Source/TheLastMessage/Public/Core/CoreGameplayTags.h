#pragma once

#include "NativeGameplayTags.h"

namespace CoreGameplayTags
{
	// 가드 코어의 입력. 코어 DA의 AbilityInputTag와 플레이어의 AbilityInputBindings에 같은 태그를 지정한다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Guard);

	// 가드 키를 누르고 있는 동안 플레이어 ASC에 붙는 상태.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Guarding);
}
