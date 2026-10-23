#include "Gimmick/FactoryVentFan.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AFactoryVentFan::AFactoryVentFan()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Rotor = CreateDefaultSubobject<USceneComponent>(TEXT("Rotor"));
    Rotor->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> HousingAsset(TEXT("/Game/Test_Jihong/Gimmicks/VentFan/SM_VentHousing.SM_VentHousing"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> RotorAsset(TEXT("/Game/Test_Jihong/Gimmicks/VentFan/SM_VentRotor.SM_VentRotor"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DetailAsset(TEXT("/Game/Test_Jihong/Gimmicks/VentFan/SM_VentDetails.SM_VentDetails"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SupportAsset(TEXT("/Game/_ExternalAssets/SciFiFactory/Meshes/SM_Column_B.SM_Column_B"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> LampAsset(TEXT("/Game/_ExternalAssets/SciFiFactory/Meshes/SM_Lamp.SM_Lamp"));
    auto Part = [&](FName Name, USceneComponent* Parent, UStaticMesh* Mesh, FVector Location, FVector Scale, FRotator Rotation, bool bSolid)
    {
        UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Component->SetupAttachment(Parent);
        Component->SetStaticMesh(Mesh);
        Component->SetRelativeLocation(Location);
        Component->SetRelativeScale3D(Scale);
        Component->SetRelativeRotation(Rotation);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
        Component->SetGenerateOverlapEvents(false);
    };
    Part(TEXT("Housing"), RootComponent, HousingAsset.Object, FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, true);
    Part(TEXT("FanBlades"), Rotor, RotorAsset.Object, FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, false);
    Part(TEXT("MotorDetails"), RootComponent, DetailAsset.Object, FVector::ZeroVector, FVector::OneVector, FRotator::ZeroRotator, false);
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        Part(*FString::Printf(TEXT("FactoryMount%d"), Side), RootComponent, SupportAsset.Object,
            FVector(Side * 135, 27, -160), FVector(.32, .32, .4), FRotator::ZeroRotator, true);
        Part(*FString::Printf(TEXT("FactoryLamp%d"), Side), RootComponent, LampAsset.Object,
            FVector(Side * 155, -30, 0), FVector(.3, .16, .3), FRotator(0, -90, 90), false);
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
