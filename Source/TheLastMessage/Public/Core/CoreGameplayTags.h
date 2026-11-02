#pragma once

#include "NativeGameplayTags.h"

namespace CoreGameplayTags
{
	// 가드 코어의 입력. 코어 DA의 AbilityInputTag와 플레이어의 AbilityInputBindings에 같은 태그를 지정한다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Guard);

	// 가드 키를 누르고 있는 동안 플레이어 ASC에 붙는 상태.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Guarding);

	// 가드가 피해를 막았을 때 방어자 ASC에 보내는 알림. EventMagnitude는 막은 피해량,
	// Instigator는 공격자, Target은 방어자이며 일부만 막았으면 TargetTags에 Result_GuardPartial을 담는다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GuardBlocked);

	// 에너지가 부족해 피해의 일부만 막은 결과. 가드 성공 이벤트의 TargetTags에만 담는다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Result_GuardPartial);
}
