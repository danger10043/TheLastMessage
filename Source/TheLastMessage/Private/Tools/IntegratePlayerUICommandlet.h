#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "IntegratePlayerUICommandlet.generated.h"

/** Explicit editor migration tool. Never runs automatically or rebuilds UI at game startup. */
UCLASS()
class UIntegratePlayerUICommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UIntegratePlayerUICommandlet();
	virtual int32 Main(const FString& Params) override;
};
