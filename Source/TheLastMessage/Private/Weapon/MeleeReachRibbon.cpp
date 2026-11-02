#include "Weapon/MeleeReachRibbon.h"
#include "Weapon/MeleeSweep.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRibbonRendererProperties.h"

UMeleeReachRibbon::UMeleeReachRibbon(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
 SetCollisionEnabled(ECollisionEnabled::NoCollision);
 SetGenerateOverlapEvents(false);
 CastShadow = false;
 bUseAsOccluder = false;
}

void UMeleeReachRibbon::Initialize(UStaticMeshComponent* Weapon, const UWeaponDataAsset* Data)
{
 WeaponMesh = Weapon;
 FVector Start, End;
 if (!MeleeSweep::ResolveReach(Data, Weapon, Start, End, LocalInner, LocalOuter)
  || LocalInner.Equals(LocalOuter)) { DestroyComponent(); return; }
 SetAbsolute(true, true, true);
 SetWorldTransform(FTransform::Identity);
 // Use the same particle and material color inputs as the weapon's ribbon.
 FLinearColor TrailColor = FLinearColor::White;
 UMaterialInterface* TrailSurface = nullptr;
 bool bUseTrailSurface = false;
 if (UNiagaraSystem* Trail = Data->MeleeTrailEffect)
 {
  const FNiagaraVariable ColorVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("User.Color"));
  TrailColor = Trail->GetExposedParameters().GetParameterValueOrDefault<FLinearColor>(ColorVariable, FLinearColor::White);
  for (const FNiagaraEmitterHandle& Handle : Trail->GetEmitterHandles())
  {
   const FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData();
   if (!Emitter) continue;
   for (UNiagaraRendererProperties* Renderer : Emitter->GetRenderers())
   {
    const auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer);
    if (!Ribbon || !Ribbon->Material) continue;
    FLinearColor Emissive;
    if (Ribbon->Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("emissive color")), Emissive))
    {
     TrailColor *= Emissive;
     const float Peak = FMath::Max3(TrailColor.R, TrailColor.G, TrailColor.B);
     if (Peak > 1.5f) TrailColor *= 1.5f / Peak;
    }
    else
    {
     TrailSurface = Ribbon->Material;
     bUseTrailSurface = true;
    }
    break;
   }
   break;
  }
 }
 Color = FLinearColor(1, 1, 1, Data->MeleeReachColor.A);
 FadeTime = FMath::Clamp(Data->MeleeReachFadeTime, 0.06f, 0.3f);
 UMaterialInterface* Material = bUseTrailSurface ? Data->MeleeReachTrailSurface.LoadSynchronous() : Data->MeleeReachMaterial.LoadSynchronous();
 if (!Material) { DestroyComponent(); return; }
 UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
 if (TrailSurface) Instance->CopyMaterialUniformParameters(TrailSurface);
 Instance->SetVectorParameterValue(TEXT("ReachColor"), TrailColor);
 SetMaterial(0, Instance);
 AddTickPrerequisiteComponent(Weapon);
 const FTransform Transform = Weapon->GetComponentTransform();
 Samples.Add({Transform.TransformPosition(LocalInner), Transform.TransformPosition(LocalOuter), Age});
}

void UMeleeReachRibbon::StopEmission() { bEmitting = false; }

void UMeleeReachRibbon::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
 Age += DeltaTime;
 if (!WeaponMesh.IsValid()) bEmitting = false;
 Samples.RemoveAll([this](const FSample& Sample) { return Age - Sample.Time > FadeTime; });
 if (bEmitting)
 {
  const FTransform Transform = WeaponMesh->GetComponentTransform();
  const FVector Inner = Transform.TransformPosition(LocalInner), Outer = Transform.TransformPosition(LocalOuter);
  if (Samples.IsEmpty() || !Samples.Last().Outer.Equals(Outer, 0.5f))
   Samples.Add({Inner, Outer, Age});
 }
 if (!bEmitting && Samples.IsEmpty()) { DestroyComponent(); return; }
 TArray<FVector> Vertices;
 TArray<int32> Triangles;
 TArray<FLinearColor> Colors;
 TArray<FVector2D> UVs;
 for (int32 Index = 0; Index < Samples.Num(); ++Index)
 {
  const FSample& Sample = Samples[Index];
  const float Fade = FMath::Square(FMath::Clamp(1.f - (Age - Sample.Time) / FadeTime, 0.f, 1.f));
  // Fill the entire added reach with a broad slash, distinct from the weapon's narrow trail.
  constexpr int32 BandCount = 4;
  const float AcrossBands[BandCount] = {0.f, 0.18f, 0.85f, 1.f};
  const float AlphaBands[BandCount] = {0.25f, 0.8f, 1.f, 0.f};
  for (int32 Band = 0; Band < BandCount; ++Band)
  {
   const float Across = AcrossBands[Band];
   Vertices.Add(FMath::Lerp(Sample.Inner, Sample.Outer, Across));
   FLinearColor Tint = Color;
   Tint.A *= Fade * AlphaBands[Band];
   Colors.Add(Tint);
   UVs.Add(FVector2D(Across, (Age - Sample.Time) / FadeTime));
  }
  if (Index > 0)
   for (int32 Band = 0; Band < BandCount - 1; ++Band)
   {
    const int32 A = (Index - 1) * BandCount + Band, B = Index * BandCount + Band;
    Triangles.Append({A, B, A + 1, A + 1, B, B + 1});
   }
 }
 if (Triangles.IsEmpty()) ClearAllMeshSections();
 else CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), UVs, Colors, TArray<FProcMeshTangent>(), false);
}
