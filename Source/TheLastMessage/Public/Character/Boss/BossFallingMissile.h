#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossFallingMissile.generated.h"

// FallingMissile movement and impact rules will be implemented separately.
UCLASS(Abstract, NotBlueprintable)
class THELASTMESSAGE_API ABossFallingMissile : public AActor
{
    GENERATED_BODY()

public:
    ABossFallingMissile();
};