#include "Interfaces/SideScrollerInterop.h"
#include "GameFramework/Actor.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

void USideScrollerInterop::SendInteraction(AActor* Target, AActor* Interactor)
{
	if (!IsValid(Target)) return;
	if (UFunction* Function = Target->FindFunction(TEXT("Interaction")))
	{
		FStructOnScope Parameters(Function);
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Function, TEXT("Interactor")))
		{
			Property->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Interactor);
			Target->ProcessEvent(Function, Parameters.GetStructMemory());
		}
	}
}

void USideScrollerInterop::UpdatePickupWidget(UObject* Target, int32 Amount)
{
	if (!IsValid(Target)) return;
	if (UFunction* Function = Target->FindFunction(TEXT("UpdateUI")))
	{
		FStructOnScope Parameters(Function);
		if (FIntProperty* Property = FindFProperty<FIntProperty>(Function, TEXT("Amount")))
		{
			Property->SetPropertyValue_InContainer(Parameters.GetStructMemory(), Amount);
			Target->ProcessEvent(Function, Parameters.GetStructMemory());
		}
	}
}
