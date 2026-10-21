#include "Map/AreaMapRegion.h"
#include "Components/BoxComponent.h"

AAreaMapRegion::AAreaMapRegion()
{
	RegionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBounds"));
	SetRootComponent(RegionBounds);
	RegionBounds->SetBoxExtent(FVector(1200,500,700));
	RegionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBounds->SetGenerateOverlapEvents(false);
	RegionBounds->SetCanEverAffectNavigation(false);
	RegionBounds->SetHiddenInGame(true);
}

void AAreaMapRegion::CaptureLayoutBounds()
{
	AuthoredLayoutBounds.Init();
	for (const AActor* Actor : LayoutActors)
	{
		if (IsValid(Actor)) AuthoredLayoutBounds += Actor->GetComponentsBoundingBox(true);
	}
}

FBox2D AAreaMapRegion::GetMapBounds() const
{
	const FBox Base = RegionBounds->Bounds.GetBox();
	FBox Current(ForceInit);
	for (const AActor* Actor : LayoutActors) if (IsValid(Actor)) Current += Actor->GetComponentsBoundingBox(true);
	if (!Current.IsValid || !AuthoredLayoutBounds.IsValid)
		return FBox2D(FVector2D(Base.Min.X,Base.Min.Z),FVector2D(Base.Max.X,Base.Max.Z));
	const FVector ReferenceSize = AuthoredLayoutBounds.GetSize();
	const FVector CurrentSize = Current.GetSize();
	const double ScaleX = ReferenceSize.X > 1 ? CurrentSize.X / ReferenceSize.X : 1;
	const double ScaleZ = ReferenceSize.Z > 1 ? CurrentSize.Z / ReferenceSize.Z : 1;
	return FBox2D(FVector2D(Current.Min.X + (Base.Min.X-AuthoredLayoutBounds.Min.X)*ScaleX,
		Current.Min.Z + (Base.Min.Z-AuthoredLayoutBounds.Min.Z)*ScaleZ),
		FVector2D(Current.Min.X + (Base.Max.X-AuthoredLayoutBounds.Min.X)*ScaleX,
		Current.Min.Z + (Base.Max.Z-AuthoredLayoutBounds.Min.Z)*ScaleZ));
}
