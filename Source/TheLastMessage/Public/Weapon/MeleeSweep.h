#pragma once
#include "CoreMinimal.h"
namespace MeleeSweep
{
 THELASTMESSAGE_API void Trace(UWorld* World,AActor* Owner,const FVector& LocalStart,const FVector& LocalEnd,float Radius,const FTransform& Previous,const FTransform& Current,TArray<FHitResult>& Hits,bool bDebug=false);
}
