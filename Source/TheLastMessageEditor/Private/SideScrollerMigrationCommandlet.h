#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SideScrollerMigrationCommandlet.generated.h"

/** Copies the reference template and replaces its core gameplay graphs with native classes. */
UCLASS()
class USideScrollerMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
