#include "Weapon/WeaponImpactFeedback.h"
#include "DataAsset/WeaponDataAsset.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

FTransform WeaponImpactFeedback::MakeTransform(const UWeaponDataAsset* Data, const FVector& Location, const FVector& Normal)
{
	const FVector Direction = Normal.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
	const FTransform Contact(Direction.Rotation(), Location);
	return Data ? Data->ImpactEffectOffset * Contact : Contact;
}

void WeaponImpactFeedback::Play(UObject* WorldContext, const UWeaponDataAsset* Data, const FVector& Location, const FVector& Normal,
	UNiagaraSystem* FallbackEffect, USoundBase* FallbackSound)
{
	UNiagaraSystem* Effect = Data && Data->ImpactEffect ? Data->ImpactEffect.Get() : FallbackEffect;
	USoundBase* Sound = Data && Data->ImpactSound ? Data->ImpactSound.Get() : FallbackSound;
	const FTransform Transform = MakeTransform(Data, Location, Normal);
	if (Effect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, Effect, Transform.GetLocation(), Transform.Rotator(), Transform.GetScale3D());
	if (Sound) UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location,
		Data ? Data->ImpactSoundVolume : 1.0f, Data ? Data->ImpactSoundPitch : 1.0f, 0.0f, Data ? Data->WeaponSoundAttenuation.Get() : nullptr);
}
