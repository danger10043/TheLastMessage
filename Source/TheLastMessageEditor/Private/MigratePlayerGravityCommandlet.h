#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MigratePlayerGravityCommandlet.generated.h"

/** Migrate the temporary BP component to PlayerBase's inherited native component. */
UCLASS()
class UMigratePlayerGravityCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UMigratePlayerGravityCommandlet();
	virtual int32 Main(const FString& Params) override;
};
