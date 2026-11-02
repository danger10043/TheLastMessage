#pragma once

#include "CoreMinimal.h"
#include "Character/Boss/BossBullet.h"
#include "BossRockProjectile.generated.h"

UCLASS()
class THELASTMESSAGE_API ABossRockProjectile : public ABossBullet
{
    GENERATED_BODY()


public:
    ABossRockProjectile();
    virtual void Tick(float DeltaSeconds) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Boss|Rock|Visual",
        meta = (Units = "deg/s"))
    FRotator MeshRotationRate = FRotator(1080.f, 0.f, 0.f);
};