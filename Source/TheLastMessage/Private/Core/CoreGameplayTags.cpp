#include "Core/CoreGameplayTags.h"

namespace CoreGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Input_Guard, "Input.Core.Guard");
	UE_DEFINE_GAMEPLAY_TAG(State_Guarding, "State.Guarding");
	UE_DEFINE_GAMEPLAY_TAG(State_GuardPerfect, "State.GuardPerfect");
	UE_DEFINE_GAMEPLAY_TAG(Event_GuardBlocked, "Event.Core.GuardBlocked");
	UE_DEFINE_GAMEPLAY_TAG(Result_GuardPartial, "Combat.Result.GuardPartial");
	UE_DEFINE_GAMEPLAY_TAG(Result_GuardPerfect, "Combat.Result.GuardPerfect");
}
