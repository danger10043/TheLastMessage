#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AssetFolderMoveCommandlet.generated.h"

/** Moves an asset folder through Unreal's rename and reference-fixup APIs. */
UCLASS()
class UAssetFolderMoveCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
