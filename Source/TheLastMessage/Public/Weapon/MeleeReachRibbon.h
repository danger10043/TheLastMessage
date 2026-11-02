#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "MeleeReachRibbon.generated.h"

class UWeaponDataAsset;
class UStaticMeshComponent;

/** A short lived ribbon over the added melee reach, sampled from the weapon's actual motion. */
UCLASS()
class THELASTMESSAGE_API UMeleeReachRibbon : public UProceduralMeshComponent
{
 GENERATED_BODY()
public:
 UMeleeReachRibbon(const FObjectInitializer& ObjectInitializer);
 void Initialize(UStaticMeshComponent* Weapon, const UWeaponDataAsset* Data);
 void StopEmission();
 virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
 struct FSample { FVector Inner, Outer; float Time; };
 TArray<FSample> Samples;
 TWeakObjectPtr<UStaticMeshComponent> WeaponMesh;
 FVector LocalInner, LocalOuter;
 FLinearColor Color;
 float Age = 0, FadeTime = 0.1f;
 bool bEmitting = true;
};
