#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreatePlayerUICommandlet.generated.h"

/** Creates editable Widget Blueprints and connects their derived HUD to GM_Test. */
UCLASS()
class UCreatePlayerUICommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCreatePlayerUICommandlet();
	virtual int32 Main(const FString& Params) override;
};
