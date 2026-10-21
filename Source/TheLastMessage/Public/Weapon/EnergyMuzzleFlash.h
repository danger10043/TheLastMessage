#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnergyMuzzleFlash.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UWeaponDataAsset;

/** Short-lived, socket-attached emissive energy pulse; entirely cosmetic. */
UCLASS(NotBlueprintable, Transient)
class THELASTMESSAGE_API AEnergyMuzzleFlash : public AActor
{
	GENERATED_BODY()
public:
	AEnergyMuzzleFlash();
	void Initialize(UWeaponDataAsset* Settings, bool bHeavy);
	virtual void Tick(float DeltaSeconds) override;
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Halo;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HaloMaterial;
	float Elapsed = 0.0f;
	float Duration = 0.1f;
	float Size = 14.0f;
	float Brightness = 12.0f;
	bool bHeavyPulse = false;
};
