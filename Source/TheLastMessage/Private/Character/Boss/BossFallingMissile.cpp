#include "Character/Boss/BossFallingMissile.h"

ABossFallingMissile::ABossFallingMissile()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);
}