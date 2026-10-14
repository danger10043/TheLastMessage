#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SideScrollerInterop.generated.h"

/** Compatibility with the template's Blueprint interfaces and presentation assets. */
UCLASS()
class THELASTMESSAGE_API USideScrollerInterop : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static void SendInteraction(AActor* Target, AActor* Interactor);
	static void UpdatePickupWidget(UObject* Target, int32 Amount);
};
