#include "Weapon/EnergyMuzzleFlash.h"
#include "DataAsset/RangedFireFeedbackData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AEnergyMuzzleFlash::AEnergyMuzzleFlash()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Halo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Halo"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (UStaticMeshComponent* Mesh : { Core.Get(), Halo.Get() })
	{
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetStaticMesh(Sphere.Object);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
	}
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(130.0f);
	SetActorHiddenInGame(true);
}

void AEnergyMuzzleFlash::Initialize(URangedFireFeedbackData* Settings, bool bHeavy)
{
	UMaterialInterface* Material = Settings ? Settings->EnergyMaterial.LoadSynchronous() : nullptr;
	if (!Material) { Destroy(); return; }
	bHeavyPulse = bHeavy;
	Size = FMath::Max(1.0f, Settings->EnergySize) * (bHeavy ? 1.35f : 1.0f);
	Brightness = FMath::Max(0.0f, Settings->EnergyBrightness);
	Duration = Settings->FlashDuration > 0.0f ? Settings->FlashDuration : 0.1f;
	const FLinearColor Color = Settings->bAutomaticEnergyColor
		? (bHeavy ? FLinearColor(0.3f, 0.12f, 1.0f) : FLinearColor(0.02f, 0.7f, 1.0f)) : Settings->EnergyColor;
	CoreMaterial = UMaterialInstanceDynamic::Create(Material, this);
	HaloMaterial = UMaterialInstanceDynamic::Create(Material, this);
	CoreMaterial->SetVectorParameterValue(TEXT("ProjectileColor"), FMath::Lerp(Color, FLinearColor::White, 0.65f));
	HaloMaterial->SetVectorParameterValue(TEXT("ProjectileColor"), Color);
	Core->SetMaterial(0, CoreMaterial); Halo->SetMaterial(0, HaloMaterial);
	Core->SetVisibility(true); Core->SetHiddenInGame(false);
	Halo->SetVisibility(true); Halo->SetHiddenInGame(false);
	Light->SetLightColor(Color);
	SetActorHiddenInGame(false);
	// The fade owns the short lifetime. A long frame must not expire the effect before it is drawn.
	SetLifeSpan(FMath::Max(5.0f, Duration * 4.0f));
	Tick(0.0f);
}

void AEnergyMuzzleFlash::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += FMath::Min(FMath::Max(DeltaSeconds, 0.0f), Duration / 3.0f);
	const float Fade = FMath::Clamp(1.0f - Elapsed / Duration, 0.0f, 1.0f);
	const float Scale = Size / 50.0f * (0.35f + Fade * 0.65f);
	Core->SetRelativeScale3D(FVector(bHeavyPulse ? 2.0f : 0.9f, 0.3f, 0.3f) * Scale);
	// Start the luminous core at the socket and extend outward instead of hiding half inside the barrel.
	Core->SetRelativeLocation(FVector(50.0f * Scale * (bHeavyPulse ? 2.0f : 0.9f), 0.0f, 0.0f));
	Halo->SetRelativeScale3D(FVector(0.1f, 0.85f, 0.85f) * Scale);
	if (CoreMaterial) CoreMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), Brightness * Fade);
	if (HaloMaterial) HaloMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), Brightness * 0.5f * Fade);
	Light->SetIntensity(800.0f * Fade);
	if (Fade < 0.05f) Destroy();
}
