#pragma once

#include "NativeGameplayTags.h"

namespace CoreGameplayTags
{
	// 가드 코어의 입력. 코어 DA의 AbilityInputTag와 플레이어의 AbilityInputBindings에 같은 태그를 지정한다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Guard);

	// 가드 키를 누르고 있는 동안 플레이어 ASC에 붙는 상태.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Guarding);

	// 가드를 시작한 직후 퍼펙트 가드로 판정하는 동안 붙는 상태. 이때 받은 피해는 에너지 없이 전부 막는다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GuardPerfect);

	// 가드 중 피해를 받았을 때 방어자 ASC에 보내는 알림. EventMagnitude는 막은 피해량(에너지가 없으면 0),
	// Instigator는 공격자, Target은 방어자이며 전부 막지 못했으면 TargetTags에 Result_GuardPartial을 담는다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GuardBlocked);

	// 에너지가 부족해 피해를 전부 막지 못한 결과(하나도 못 막은 경우 포함). 가드 이벤트의 TargetTags에만 담는다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Result_GuardPartial);

	// 퍼펙트 가드로 막은 결과. 가드 이벤트의 TargetTags에만 담는다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Result_GuardPerfect);

	// 회복 GE(GE_Core_Heal)에 SetByCaller로 넘기는 회복량. 전력 흡수·잔여 전력 회수가 함께 쓴다.
	THELASTMESSAGE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_HealAmount);
}
