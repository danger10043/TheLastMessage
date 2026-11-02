#include "Weapon/MeleeSweep.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DataAsset/WeaponDataAsset.h"

bool MeleeSweep::ResolveReach(const UWeaponDataAsset* Data, const UStaticMeshComponent* Mesh,
 FVector& Start, FVector& End, FVector& Tip, FVector& ExtendedTip)
{
 if (!Data || !Mesh || !Mesh->GetStaticMesh()) return false;
 Start = Data->MeleeTraceStart; End = Data->MeleeTraceEnd;
 bool bStartIsTip = false;
 if (!Data->bCustomMeleeTrace)
 {
  const FBox Bounds = Mesh->GetStaticMesh()->GetBoundingBox();
  const FVector Extent = Bounds.GetExtent();
  const int32 Axis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? 0 : (Extent.Y >= Extent.Z ? 1 : 2);
  Start = End = Bounds.GetCenter();
  Start[Axis] = FMath::Lerp(Bounds.Min[Axis], Bounds.Max[Axis], 0.15f);
  End[Axis] = FMath::Lerp(Bounds.Min[Axis], Bounds.Max[Axis], 0.95f);
  if (Data->WeaponType == EWeaponType::HeavyMelee)
  {
   bStartIsTip = Data->MeleeTrailOffset.GetLocation()[Axis] < Bounds.GetCenter()[Axis];
   if (bStartIsTip) { Start[Axis] = Bounds.Min[Axis]; End[Axis] = FMath::Lerp(Bounds.Min[Axis], Bounds.Max[Axis], 0.35f); }
   else Start[Axis] = FMath::Lerp(Bounds.Min[Axis], Bounds.Max[Axis], 0.65f);
  }
 }
 else bStartIsTip = Start.SizeSquared() > End.SizeSquared();
 Tip = bStartIsTip ? Start : End;
 const FVector Base = bStartIsTip ? End : Start;
 const FVector Direction = (Tip - Base).GetSafeNormal();
 const float WorldScale = Mesh->GetComponentTransform().TransformVector(Direction).Length();
 const float RatioDistance = FVector::Distance(Tip, Base) * FMath::Clamp(Data->MeleeReachExtension, 0.f, 1.f);
 const float FixedDistance = WorldScale > SMALL_NUMBER ? FMath::Clamp(Data->MeleeReachExtraDistance, 0.f, 300.f) / WorldScale : 0.f;
 ExtendedTip = Tip + Direction * FMath::Max(RatioDistance, FixedDistance);
 if (bStartIsTip) Start = ExtendedTip; else End = ExtendedTip;
 return true;
}
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
