#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BossCharacterMovementComponent.generated.h"

UCLASS()
class THELASTMESSAGE_API UBossCharacterMovementComponent
    : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    bool bJumpAttackActive = false;

    virtual void ProcessLanded(
        const FHitResult& Hit, float RemainingTime, int32 Iterations) override;
};