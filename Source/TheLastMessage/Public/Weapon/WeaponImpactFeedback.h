#pragma once
#include "CoreMinimal.h"
class UWeaponDataAsset;
class UNiagaraSystem;
class USoundBase;

/** Shared contact presentation for melee sweeps and projectile collisions. */
namespace WeaponImpactFeedback
{
	FTransform MakeTransform(const UWeaponDataAsset* Data, const FVector& Location, const FVector& Normal);
	void Play(UObject* WorldContext, const UWeaponDataAsset* Data, const FVector& Location, const FVector& Normal,
		UNiagaraSystem* FallbackEffect = nullptr, USoundBase* FallbackSound = nullptr);
}
