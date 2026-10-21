#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraRibbonRendererProperties.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "EdGraph/EdGraphSchema.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "DataAsset/WeaponDataAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_MeleeHit.h"
#include "Animation/AnimNotifyState_MeleeTrail.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

namespace
{
UNiagaraNodeOutput* FindTrailOutput(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
{
	for (UEdGraphNode* Node : Graph->Nodes)
		if (UNiagaraNodeOutput* Output = Cast<UNiagaraNodeOutput>(Node))
			if (Output->GetUsage() == Usage) return Output;
	return nullptr;
}
bool SaveTrailAsset(UObject* Asset)
{
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Asset->GetOutermost(), Asset,
		*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

UNiagaraSystem* CreateTrail(const TCHAR* Name, float Width, float Lifetime)
{
	const FString Path = FString(TEXT("/Game/Main/Weapons/Effects/")) + Name;
	if (UNiagaraSystem* Existing = LoadObject<UNiagaraSystem>(nullptr, *(Path + TEXT(".") + Name))) return Existing;
	UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr,
		TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal"));
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Main/Weapons/Effects/M_MeleeTrail.M_MeleeTrail"));
	if (!Template || !Material) return nullptr;
	UNiagaraSystem* System = NewObject<UNiagaraSystem>(CreatePackage(*Path), FName(Name), RF_Public | RF_Standalone);
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
	FNiagaraEmitterHandle& Handle = System->GetEmitterHandles().Last();
	FVersionedNiagaraEmitter Instance = Handle.GetInstance();
	FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
	Data->bLocalSpace = false;
	const TArray<UNiagaraRendererProperties*> OldRenderers = Data->GetRenderers();
	for (UNiagaraRendererProperties* Renderer : OldRenderers) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
	UNiagaraRibbonRendererProperties* Ribbon = NewObject<UNiagaraRibbonRendererProperties>(Instance.Emitter);
	Ribbon->Material = Material;
	Ribbon->FacingMode = ENiagaraRibbonFacingMode::Screen;
	Instance.Emitter->AddRenderer(Ribbon, Instance.Version);
	UNiagaraGraph* Graph = CastChecked<UNiagaraScriptSource>(Data->GraphSource)->NodeGraph;
	UNiagaraNodeOutput* EmitterUpdate = FindTrailOutput(Graph, ENiagaraScriptUsage::EmitterUpdateScript);
	UNiagaraScript* SpawnRate = LoadObject<UNiagaraScript>(nullptr, TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"));
	if (!EmitterUpdate || !SpawnRate) return nullptr;
	UNiagaraNodeFunctionCall* RateNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(SpawnRate, *EmitterUpdate);
	const auto Input = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(FName(TEXT("Module.SpawnRate"))), RateNode);
	UEdGraphPin& RatePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
		*RateNode, Input, FNiagaraTypeDefinition::GetFloatDef(), FGuid(), FGuid());
	RatePin.GetSchema()->TrySetDefaultValue(RatePin, TEXT("180.0"));
	UNiagaraNodeOutput* ParticleSpawn = FindTrailOutput(Graph, ENiagaraScriptUsage::ParticleSpawnScript);
	if (!ParticleSpawn) return nullptr;
	TArray<FNiagaraVariable> Variables = {
		FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Lifetime")),
		FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.RibbonWidth")),
		FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Particles.Color")) };
	TArray<FString> Values = { FString::SanitizeFloat(Lifetime), FString::SanitizeFloat(Width), TEXT("(R=0.75,G=0.8,B=0.85,A=0.55)") };
	FNiagaraStackGraphUtilities::AddParameterModuleToStack(Variables, *ParticleSpawn, INDEX_NONE, Values);
	static_cast<UEdGraph*>(Graph)->NotifyGraphChanged();
	System->RequestCompile(true);
	System->WaitForCompilationComplete(true, false);
	if (!System->IsValid()) return nullptr;
	FAssetRegistryModule::AssetCreated(System);
	System->MarkPackageDirty();
	return SaveTrailAsset(System) ? System : nullptr;
}

bool AddTrailWindows(UAnimMontage* Montage, bool bHeavy)
{
	if (!Montage) return true;
	TArray<float> HitTimes;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
		if (Event.Notify && Event.Notify->IsA<UAnimNotify_MeleeHit>()) HitTimes.Add(Event.GetTriggerTime());
	if (HitTimes.IsEmpty()) return false;
	Montage->Modify();
	Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& Event) { return Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_MeleeTrail>(); });
	for (float HitTime : HitTimes)
	{
		float SectionStart = 0.0f, SectionEnd = Montage->GetPlayLength();
		const int32 Section = Montage->GetSectionIndexFromPosition(HitTime);
		if (Section != INDEX_NONE) Montage->GetSectionStartAndEndTime(Section, SectionStart, SectionEnd);
		const float Start = FMath::Max(SectionStart + 0.01f, HitTime - (bHeavy ? 0.20f : 0.12f));
		const float End = FMath::Min(SectionEnd - 0.01f, HitTime + (bHeavy ? 0.14f : 0.10f));
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyName = TEXT("Melee Trail");
		Event.NotifyStateClass = NewObject<UAnimNotifyState_MeleeTrail>(Montage);
		Event.Link(Montage, Start);
		Event.SetDuration(FMath::Max(0.01f, End - Start));
		Event.EndLink.Link(Montage, End);
	}
	Montage->PostEditChange();
	return SaveTrailAsset(Montage);
}

void AuthorMeleeTrails()
{
	UNiagaraSystem* Pipe = CreateTrail(TEXT("NS_MeleeTrail_Pipe"), 2.8f, 0.075f);
	UNiagaraSystem* Hammer = CreateTrail(TEXT("NS_MeleeTrail_Hammer"), 5.0f, 0.11f);
	if (!Pipe || !Hammer) { UE_LOG(LogTemp, Error, TEXT("MELEE_TRAIL_AUTHOR_FAILED Niagara compilation")); return; }
	const TCHAR* Paths[] = { TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Pipe"), TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Hammer"), TEXT("/Game/Test_Inwoo/DA_Weapon_Pipe1") };
	for (const TCHAR* Path : Paths)
	{
		UWeaponDataAsset* Data = LoadObject<UWeaponDataAsset>(nullptr, Path);
		if (!Data || !Data->WeaponMesh) { UE_LOG(LogTemp, Error, TEXT("MELEE_TRAIL_AUTHOR_FAILED %s"), Path); return; }
		const bool bHeavy = Data->WeaponType == EWeaponType::HeavyMelee;
		if (!AddTrailWindows(Data->AttackMontage, bHeavy)) { UE_LOG(LogTemp, Error, TEXT("MELEE_TRAIL_AUTHOR_FAILED no melee hit notify %s"), Path); return; }
		const FBoxSphereBounds Bounds = Data->WeaponMesh->GetBounds();
		FVector Tip = Bounds.Origin;
		int32 Axis = 0;
		if (Bounds.BoxExtent.Y > Bounds.BoxExtent.X) Axis = 1;
		if (Bounds.BoxExtent.Z > Bounds.BoxExtent[Axis]) Axis = 2;
		const float Min = Bounds.Origin[Axis] - Bounds.BoxExtent[Axis];
		const float Max = Bounds.Origin[Axis] + Bounds.BoxExtent[Axis];
		Tip[Axis] = (FMath::Abs(Min) > FMath::Abs(Max) ? Min : Max) * 0.9f;
		Data->MeleeTrailEffect = bHeavy ? Hammer : Pipe;
		Data->bUseMeleeTrailNotifyWindow = Data->AttackMontage != nullptr;
		Data->MeleeTrailOffset = FTransform(Tip);
		Data->MeleeTrailSocketName = NAME_None;
		Data->MeleeTrailDuration = bHeavy ? 0.34f : 0.22f;
		if (!SaveTrailAsset(Data)) { UE_LOG(LogTemp, Error, TEXT("MELEE_TRAIL_AUTHOR_FAILED save %s"), Path); return; }
		UE_LOG(LogTemp, Display, TEXT("MELEE_TRAIL_CONFIGURED %s tip=%s"), Path, *Tip.ToString());
	}
	UE_LOG(LogTemp, Display, TEXT("MELEE_TRAIL_AUTHOR_SUCCESS"));
}
FAutoConsoleCommand AuthorTrailsCommand(TEXT("tlm.AuthorMeleeTrails"), TEXT("Create the project's short melee ribbon trails and animation windows"), FConsoleCommandDelegate::CreateStatic(&AuthorMeleeTrails));

void PreviewMeleeTrails()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || !FApp::CanEverRender()) return;
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(FVector(0, -450, 10000), FRotator(0, 90, 0));
	USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
	Target->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	Target->ClearColor = FLinearColor(0.015f, 0.015f, 0.025f);
	Target->InitAutoFormat(1024, 512);
	Target->UpdateResourceImmediate(true);
	Capture->TextureTarget = Target;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->bCaptureEveryFrame = false;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetTonemapper(false);
	Capture->ShowFlags.SetLighting(false);
	AStaticMeshActor* Reference = World->SpawnActor<AStaticMeshActor>(FVector(0, 0, 10000), FRotator::ZeroRotator);
	Reference->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Reference->SetActorScale3D(FVector(0.3f));
	Capture->ShowOnlyComponent(Reference->GetStaticMeshComponent());
	TArray<UNiagaraComponent*> Components;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const TCHAR* Name = Index == 0 ? TEXT("NS_MeleeTrail_Pipe") : TEXT("NS_MeleeTrail_Hammer");
		UWeaponDataAsset* PreviewWeapon = LoadObject<UWeaponDataAsset>(nullptr, Index == 0
			? TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Pipe") : TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Hammer"));
		UNiagaraSystem* Asset = PreviewWeapon ? PreviewWeapon->MeleeTrailEffect.Get() : nullptr;
		if (!Asset) return;
		Asset->WaitForCompilationComplete(true, false);
		UNiagaraComponent* Component = NewObject<UNiagaraComponent>(Reference);
		Component->SetAsset(Asset);
		Component->SetWorldLocation(FVector(0, 0, 10000));
		Component->RegisterComponentWithWorld(World);
		if (!Component) { UE_LOG(LogTemp, Error, TEXT("MELEE_TRAIL_PREVIEW_FAILED spawn")); return; }
		Component->SetForceSolo(true);
		Component->Activate(true);
		Capture->ShowOnlyComponent(Component);
		Components.Add(Component);
		for (int32 Frame = 0; Frame < 40; ++Frame)
		{
			const float Angle = -1.0f + Frame * 0.05f;
			Component->SetWorldLocation(FVector((Index == 0 ? -110 : 110) + 75 * FMath::Sin(Angle), 0, 10000 + 75 * FMath::Cos(Angle)));
			Component->AdvanceSimulation(1, 1.0f / 180.0f);
		}
		Component->RecreateRenderState_Concurrent();
		Component->MarkRenderDynamicDataDirty();
		if (Component->GetSystemInstanceController().IsValid())
		{
			Component->GetSystemInstanceController()->WaitForConcurrentTickAndFinalize();
			for (const FNiagaraEmitterInstanceRef& Emitter : Component->GetSystemInstanceController()->GetSystemInstance_Unsafe()->GetEmitters())
				UE_LOG(LogTemp, Display, TEXT("MELEE_TRAIL_PREVIEW_PARTICLES %s count=%d bounds=%s"), Name, Emitter->GetNumParticles(), *Component->Bounds.ToString());
		}
		UE_LOG(LogTemp, Display, TEXT("MELEE_TRAIL_PREVIEW_STATE %s valid=%d ready=%d controller=%d age=%f"), Name,
			Asset->IsValid(), Asset->IsReadyToRun(), Component->GetSystemInstanceController().IsValid(),
			Component->GetSystemInstanceController().IsValid() ? Component->GetSystemInstanceController()->GetAge() : -1.0f);
	}
	World->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	Capture->CaptureScene();
	FlushRenderingCommands();
	TArray<FColor> Pixels;
	Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(1024, 512, Pixels, Png);
	const FString Filename = FPaths::ProjectSavedDir() / TEXT("MeleeTrailPreview.png");
	FFileHelper::SaveArrayToFile(Png, *Filename);
	for (UNiagaraComponent* Component : Components) Component->DestroyComponent();
	Camera->Destroy();
	Reference->Destroy();
	UE_LOG(LogTemp, Display, TEXT("MELEE_TRAIL_PREVIEW_SAVED %s"), *Filename);
}
FAutoConsoleCommand PreviewTrailsCommand(TEXT("tlm.PreviewMeleeTrails"), TEXT("Render a comparison of the two melee ribbon assets"), FConsoleCommandDelegate::CreateStatic(&PreviewMeleeTrails));

void AuthorSoftTofuTrails()
{
	UNiagaraSystem* Source = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/_ExternalAssets/SlashTrail_SoftTofu/Niagara/Basic/NS_SlashTrail_Basic_Loop.NS_SlashTrail_Basic_Loop"));
	if (!Source) { UE_LOG(LogTemp, Error, TEXT("SOFTTOFU_TRAIL_FAILED missing source")); return; }
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const bool bHeavy = Index == 1;
		const FString Name = bHeavy ? TEXT("NS_MeleeTrail_Hammer_SoftTofu") : TEXT("NS_MeleeTrail_Pipe_SoftTofu");
		const FString PackagePath = TEXT("/Game/Main/Weapons/Effects/") + Name;
		UNiagaraSystem* Trail = LoadObject<UNiagaraSystem>(nullptr, *(PackagePath + TEXT(".") + Name));
		if (!Trail)
		{
			Trail = CastChecked<UNiagaraSystem>(StaticDuplicateObject(Source, CreatePackage(*PackagePath), FName(*Name)));
			Trail->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Trail);
		}
		TArray<FNiagaraVariable> Parameters;
		Trail->GetExposedParameters().GetParameters(Parameters);
		int32 Changed = 0;
		for (const FNiagaraVariable& Parameter : Parameters)
		{
			const FString Key = Parameter.GetName().ToString();
			UE_LOG(LogTemp, Display, TEXT("SOFTTOFU_PARAMETER %s %s %s"), *Name, *Key, *Parameter.GetType().GetName());
			if (Parameter.GetType() == FNiagaraTypeDefinition::GetFloatDef())
			{
				TOptional<float> Value;
				if (Key == TEXT("User.Trail Width")) Value = bHeavy ? 75.0f : 45.0f;
				else if (Key == TEXT("User.Lifetime_SlashTrail")) Value = bHeavy ? 0.38f : 0.26f;
				else if (Key == TEXT("User.SpawnRate_SlashTrail")) Value = 240.0f;
				if (Value.IsSet()) { Trail->GetExposedParameters().SetParameterValue(Value.GetValue(), Parameter); ++Changed; }
			}
			else if (Parameter.GetType() == FNiagaraTypeDefinition::GetColorDef() && Key == TEXT("User.Color_"))
			{
				Trail->GetExposedParameters().SetParameterValue(
					bHeavy ? FLinearColor(2.2f, 1.5f, 0.8f, 1.0f) : FLinearColor(0.9f, 1.4f, 2.2f, 1.0f), Parameter);
				++Changed;
			}
		}
		if (Changed < 3) { UE_LOG(LogTemp, Error, TEXT("SOFTTOFU_TRAIL_FAILED unexpected parameters")); return; }
		Trail->RequestCompile(true);
		Trail->WaitForCompilationComplete(true, false);
		if (!Trail->IsValid() || !SaveTrailAsset(Trail)) { UE_LOG(LogTemp, Error, TEXT("SOFTTOFU_TRAIL_FAILED compile/save")); return; }
		const TArray<FString> WeaponPaths = bHeavy
			? TArray<FString>{ TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Hammer") }
			: TArray<FString>{ TEXT("/Game/Test_Jihong/DataAsset/DA_Weapon_Pipe"), TEXT("/Game/Test_Inwoo/DA_Weapon_Pipe1") };
		for (const FString& WeaponPath : WeaponPaths)
		{
			UWeaponDataAsset* Data = LoadObject<UWeaponDataAsset>(nullptr, *WeaponPath);
			if (!Data) { UE_LOG(LogTemp, Error, TEXT("SOFTTOFU_TRAIL_FAILED missing weapon")); return; }
			Data->MeleeTrailEffect = Trail;
			if (!SaveTrailAsset(Data)) { UE_LOG(LogTemp, Error, TEXT("SOFTTOFU_TRAIL_FAILED weapon save")); return; }
			UE_LOG(LogTemp, Display, TEXT("SOFTTOFU_TRAIL_ASSIGNED %s %s"), *WeaponPath, *Name);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("SOFTTOFU_TRAIL_SUCCESS"));
}
FAutoConsoleCommand AuthorSoftTofuCommand(TEXT("tlm.AuthorSoftTofuTrails"), TEXT("Create per-weapon SlashTrail SoftTofu presets without modifying the source pack"), FConsoleCommandDelegate::CreateStatic(&AuthorSoftTofuTrails));
}
#endif
