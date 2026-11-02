#pragma once
#include "CoreMinimal.h"
class UWeaponDataAsset;
class UStaticMeshComponent;
namespace MeleeSweep
{
 THELASTMESSAGE_API bool ResolveReach(const UWeaponDataAsset* Data, const UStaticMeshComponent* Mesh, FVector& Start, FVector& End, FVector& Tip, FVector& ExtendedTip);
 THELASTMESSAGE_API void Trace(UWorld* World,AActor* Owner,const FVector& LocalStart,const FVector& LocalEnd,float Radius,const FTransform& Previous,const FTransform& Current,TArray<FHitResult>& Hits,bool bDebug=false);
}
