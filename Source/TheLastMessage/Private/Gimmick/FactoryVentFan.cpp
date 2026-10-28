#include "Gimmick/FactoryVentFan.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AFactoryVentFan::AFactoryVentFan()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Rotor = CreateDefaultSubobject<USceneComponent>(TEXT("Rotor"));
    Rotor->SetupAttachment(RootComponent);
    auto Part = [&](FName Name, USceneComponent* Parent, FVector Location, FVector Scale, FRotator Rotation, bool bSolid)
    {
        UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Component->SetupAttachment(Parent);
        Component->SetRelativeLocation(Location);
        Component->SetRelativeScale3D(Scale);
        Component->SetRelativeRotation(Rotation);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
        Component->SetGenerateOverlapEvents(false);
    };
    Part(TEXT("Housing"), RootComponent, FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, true);
    Part(TEXT("FanBlades"), Rotor, FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, false);
    Part(TEXT("MotorDetails"), RootComponent, FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, false);
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        Part(*FString::Printf(TEXT("FactoryMount%d"), Side), RootComponent,
            FVector(Side * 135, 27, -160), FVector(.32, .32, .4), FRotator::ZeroRotator, true);
        Part(*FString::Printf(TEXT("FactoryLamp%d"), Side), RootComponent,
            FVector(Side * 155, -30, 0), FVector(.3, .16, .3), FRotator(0, -90, 90), false);
    }
}

void AFactoryVentFan::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    TInlineComponentArray<UStaticMeshComponent*> Parts(this);
    for (UStaticMeshComponent* Part : Parts)
    {
        const FString Name = Part->GetName();
        if (Name == TEXT("Housing")) Part->SetStaticMesh(HousingMesh);
        else if (Name == TEXT("FanBlades")) Part->SetStaticMesh(BladesMesh);
        else if (Name == TEXT("MotorDetails")) Part->SetStaticMesh(MotorMesh);
        else if (Name.StartsWith(TEXT("FactoryMount"))) Part->SetStaticMesh(MountMesh);
        else if (Name.StartsWith(TEXT("FactoryLamp"))) Part->SetStaticMesh(LampMesh);
    }
}

void AFactoryVentFan::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bEnabled && FMath::IsFinite(RevolutionsPerMinute))
    {
        Rotor->AddLocalRotation(FRotator(RevolutionsPerMinute * 6.0f * DeltaSeconds, 0, 0));
    }
}
