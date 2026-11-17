#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "FixAirAttackCommandlet.generated.h"

/** Explicit, offline animation migration and inspection. */
UCLASS()
class UFixAirAttackCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
