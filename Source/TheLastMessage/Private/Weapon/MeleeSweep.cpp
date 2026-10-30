#include "Weapon/MeleeSweep.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
void MeleeSweep::Trace(UWorld* World,AActor* Owner,const FVector& LocalStart,const FVector& LocalEnd,float Radius,const FTransform& Previous,const FTransform& Current,TArray<FHitResult>& Hits,bool bDebug)
{
 if(!World || Radius<=0) return;
 const float Scale=FMath::Max(Previous.GetScale3D().GetAbsMax(),Current.GetScale3D().GetAbsMax());
 const int32 Samples=FMath::Clamp(FMath::CeilToInt(FVector::Distance(LocalStart,LocalEnd)*Scale/Radius),1,24);
 const float Angle=Previous.GetRotation().AngularDistance(Current.GetRotation());
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(Angle/FMath::DegreesToRadians(10.f)),1,18);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(WeaponPathSweep),false,Owner);
 FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
 for(int32 Step=0;Step<Steps;++Step)
 {
  FTransform A,B; A.Blend(Previous,Current,float(Step)/Steps);B.Blend(Previous,Current,float(Step+1)/Steps);
  for(int32 Index=0;Index<=Samples;++Index)
  {
   const FVector Local=FMath::Lerp(LocalStart,LocalEnd,float(Index)/Samples);
   const FVector Start=A.TransformPosition(Local), End=B.TransformPosition(Local);
   TArray<FHitResult> SegmentHits;
   World->SweepMultiByObjectType(SegmentHits,Start,End,FQuat::Identity,Objects,FCollisionShape::MakeSphere(Radius),Query);
   Hits.Append(SegmentHits);
#if ENABLE_DRAW_DEBUG
   if(bDebug){DrawDebugLine(World,Start,End,FColor::Cyan,false,0.5f);DrawDebugSphere(World,End,Radius,8,FColor::Cyan,false,0.5f);}
#endif
  }
 }
}
