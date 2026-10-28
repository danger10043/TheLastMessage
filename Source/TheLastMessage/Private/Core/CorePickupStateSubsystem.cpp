#include "Core/CorePickupStateSubsystem.h"

bool UCorePickupStateSubsystem::IsPickupConsumed(FGuid PickupId) const
{
	return PickupId.IsValid() && PickupStates.FindRef(PickupId);
}

void UCorePickupStateSubsystem::SetPickupConsumed(FGuid PickupId, bool bConsumed)
{
	if (PickupId.IsValid()) PickupStates.Add(PickupId, bConsumed);
}

void UCorePickupStateSubsystem::RestorePickupStates(const TMap<FGuid, bool>& SavedStates)
{
	PickupStates.Reset();
	for (const TPair<FGuid, bool>& State : SavedStates)
	{
		if (State.Key.IsValid()) PickupStates.Add(State.Key, State.Value);
	}
}
