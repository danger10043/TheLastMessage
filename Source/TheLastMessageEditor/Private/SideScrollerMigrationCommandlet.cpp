#include "SideScrollerMigrationCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "Engine/LevelScriptActor.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/SideScrollingCameraManager.h"
#include "Framework/SideScrollingGameMode.h"
#include "Framework/SideScrollingPlayerController.h"
#include "Character/Player/RobotPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "EdGraphSchema_K2.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"

namespace SideScrollerMigration
{
const FString SourceRoot = TEXT("/Game/_ExternalAssets/ThirdPerson/Variant_SideScroller");
const FString TargetRoot = TEXT("/Game/Test_Jihong/SideScroller");

void SetObject(UObject* Object, const FName Name, UObject* Value)
{
	FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name);
	checkf(Property, TEXT("Missing native property: %s"), *Name.ToString());
	Property->SetObjectPropertyValue_InContainer(Object, Value);
}

bool Compile(UBlueprint* Blueprint)
{
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	if (Results.NumErrors > 0 || Blueprint->Status == BS_Error)
	{
		UE_LOG(LogTemp, Error, TEXT("Migration: compilation failed: %s"), *Blueprint->GetPathName());
		return false;
	}
	return true;
}

bool Save(UObject* Asset)
{
	UPackage* Package = Asset->GetOutermost();
	Package->MarkPackageDirty();
	const bool bMap = Asset->IsA<UWorld>();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),
		bMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Arguments;
	Arguments.TopLevelFlags = RF_Public | RF_Standalone;
	Arguments.SaveFlags = SAVE_None;
	return UPackage::SavePackage(Package, Asset, *Filename, Arguments);
}

void AddBridge(UBlueprint* Blueprint, UClass* Interface, const FName EventName,
	UClass* NativeClass, const FName NativeFunction, int32 Row)
{
	check(Interface->FindFunctionByName(EventName));
	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	UK2Node_Event* Event = NewObject<UK2Node_Event>(Graph);
	Event->EventReference.SetExternalMember(EventName, Interface);
	Event->bOverrideFunction = true;
	Event->NodePosY = Row * 220;
	Graph->AddNode(Event, false, false);
	Event->CreateNewGuid();
	Event->AllocateDefaultPins();

	UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
	Call->FunctionReference.SetExternalMember(NativeFunction, NativeClass);
	Call->NodePosX = 400;
	Call->NodePosY = Event->NodePosY;
	Call->NodeComment = TEXT("Template interface adapter; gameplay runs in C++.");
	Graph->AddNode(Call, false, false);
	Call->CreateNewGuid();
	Call->AllocateDefaultPins();
	check(Schema->TryCreateConnection(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then), Call->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
	for (UEdGraphPin* Pin : Event->Pins)
	{
		if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			if (UEdGraphPin* Input = Call->FindPin(Pin->PinName, EGPD_Input))
			{
				check(Schema->TryCreateConnection(Pin, Input));
			}
		}
	}
}

UBlueprint* CreateCore(UBlueprint* Source, UClass* Parent, const FString& Name)
{
	const FString Folder = TargetRoot / TEXT("Blueprints");
	UPackage* Package = CreatePackage(*(Folder / Name));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, FName(*Name), BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("SideScrollerMigration"));
	// Only interface forwarding remains in BP. Native input and gameplay never execute twice.
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs) FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::MarkTransient);
	UEdGraph* EventGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("EventGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, EventGraph);
	FAssetRegistryModule::AssetCreated(Blueprint);
	return Blueprint;
}

/** Bake the reference's external actors into a normal level, retaining all template geometry. */
UWorld* CopyTemplateWorld(UWorld* Source, TMap<UObject*, UObject*>& Replacements)
{
	const FString PackageName = TargetRoot / Source->GetName();
	if (UWorld* ExistingWorld = FindObject<UWorld>(nullptr, *(PackageName + TEXT(".") + Source->GetName())))
	{
		ExistingWorld->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		ExistingWorld->ClearFlags(RF_Public | RF_Standalone);
	}
	UPackage* Package = CreatePackage(*PackageName);
	// This package is wholly rebuilt from the reference's loaded actors, rather than
	// saving a partially loaded version of the previous generated map.
	Package->MarkAsFullyLoaded();
	UWorld* Target = UWorld::CreateWorld(EWorldType::Editor, false, Source->GetFName(), Package, true, ERHIFeatureLevel::Num, nullptr, true);
	Target->SetFlags(RF_Public | RF_Standalone);
	Target->ClearFlags(RF_Transient);
	Target->AddToRoot();
	Replacements.Add(Source, Target);
	Replacements.Add(Source->PersistentLevel, Target->PersistentLevel);
	Replacements.Add(Source->GetWorldSettings(), Target->GetWorldSettings());
	UEngine::CopyPropertiesForUnrelatedObjects(Source->GetWorldSettings(), Target->GetWorldSettings());
	Target->GetWorldSettings()->SetWorldPartition(nullptr);
	Target->PersistentLevel->SetUseExternalActors(false);
	TSet<AActor*> Actors;
	FWorldPartitionHelpers::FForEachActorWithLoadingResult Loaded;
	bool bInitializedPartition = false;
	if (UWorldPartition* Partition = Source->GetWorldPartition())
	{
		if (!Partition->IsInitialized())
		{
			Partition->Initialize(Source, FTransform::Identity);
			bInitializedPartition = true;
		}
		FWorldPartitionHelpers::FForEachActorWithLoadingParams Loading;
		Loading.bKeepReferences = true;
		Loading.bGCPerActor = false;
		FWorldPartitionHelpers::ForEachActorWithLoading(Partition, [&Actors](const FWorldPartitionActorDescInstance* Desc)
		{
			if (AActor* Actor = Desc->GetActor()) Actors.Add(Actor);
			return true;
		}, Loading, Loaded);
	}
	for (AActor* Actor : Source->PersistentLevel->Actors) if (Actor) Actors.Add(Actor);
	int32 Count = 0;
	for (AActor* Actor : Actors)
	{
		if (Actor->IsA<AWorldSettings>() || Actor->IsA<ALevelScriptActor>() || Actor == Source->GetDefaultBrush() || Actor->IsEditorOnly()) continue;
		AActor* Copy;
		if (UObject* const* ReplacementClass = Replacements.Find(Actor->GetClass()))
		{
			FActorSpawnParameters Spawn;
			Spawn.Name = Actor->GetFName();
			Spawn.OverrideLevel = Target->PersistentLevel;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Spawn.bDeferConstruction = true;
			Copy = Target->SpawnActor<AActor>(CastChecked<UClass>(*ReplacementClass), Actor->GetActorTransform(), Spawn);
			UEngine::CopyPropertiesForUnrelatedObjects(Actor, Copy);
		}
		else
		{
			Copy = DuplicateObject<AActor>(Actor, Target->PersistentLevel, Actor->GetFName());
			Target->PersistentLevel->Actors.Add(Copy);
		}
		Copy->SetPackageExternal(false, false);
		Replacements.Add(Actor, Copy);
		++Count;
	}
	TArray<UObject*> Objects;
	GetObjectsWithPackage(Package, Objects, EGetObjectsFlags::IncludeNestedObjects);
	for (UObject* Object : Objects)
	{
		FArchiveReplaceObjectRef<UObject> Archive(Object, Replacements,
			EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
	}
	FAssetRegistryModule::AssetCreated(Target);
	UE_LOG(LogTemp, Display, TEXT("Migration: copied %d template level actors into %s"), Count, *PackageName);
	Loaded.ActorReferences.Empty();
	if (bInitializedPartition) Source->GetWorldPartition()->Uninitialize();
	return Target;
}
}

int32 USideScrollerMigrationCommandlet::Main(const FString& Params)
{
	using namespace SideScrollerMigration;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*SourceRoot), Assets, true);
	if (Assets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Side-scroller template is missing: %s"), *SourceRoot);
		return 1;
	}
	TArray<FAssetData> Existing;
	Registry.GetAssetsByPath(FName(*TargetRoot), Existing, true);
	if (Params.Contains(TEXT("FixMap")))
	{
		TMap<UObject*, UObject*> Replacements;
		UWorld* SourceWorld = nullptr;
		for (const FAssetData& Data : Assets)
		{
			if (Data.PackagePath.ToString().Contains(TEXT("/Blueprints/AI")) || Data.AssetName.ToString().StartsWith(TEXT("BPI_"))) continue;
			UObject* Original = Data.GetAsset();
			if (UWorld* World = Cast<UWorld>(Original)) { SourceWorld = World; continue; }
			FString Path = Data.GetSoftObjectPath().ToString().Replace(*SourceRoot, *TargetRoot);
			Path.ReplaceInline(TEXT("BP_SideScrollingCharacter"), TEXT("BP_RobotPlayerCharacter"));
			UObject* Copy = LoadObject<UObject>(nullptr, *Path);
			if (!Copy) continue;
			Replacements.Add(Original, Copy);
			if (UBlueprint* BP = Cast<UBlueprint>(Original)) Replacements.Add(BP->GeneratedClass, CastChecked<UBlueprint>(Copy)->GeneratedClass);
		}
		if (!SourceWorld) return 1;
		UWorld* Map = CopyTemplateWorld(SourceWorld, Replacements);
		Map->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr, *(TargetRoot / TEXT("Blueprints/BP_SideScrollingGameMode.BP_SideScrollingGameMode_C")));
		return Save(Map) ? 0 : 1;
	}
	if (!Existing.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Migration target already contains assets; refusing to overwrite: %s"), *TargetRoot);
		return 1;
	}

	IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	TMap<UObject*, UObject*> Replacements;
	TArray<UObject*> Copies;
	TMap<FString, UBlueprint*> Core;
	UWorld* SourceWorld = nullptr;
	for (const FAssetData& Data : Assets)
	{
		// Interfaces and NPC StateTrees are shared presentation/gameplay dependencies.
		// StateTree instance layouts must stay paired with their original task classes.
		if (Data.PackagePath.ToString().Contains(TEXT("/Blueprints/AI")) || Data.AssetName.ToString().StartsWith(TEXT("BPI_"))) continue;
		UObject* Source = Data.GetAsset();
		if (!Source) return 1;
		if (UWorld* World = Cast<UWorld>(Source)) { SourceWorld = World; continue; }
		UObject* Copy = nullptr;
		const FString Name = Source->GetName();
		if (Name == TEXT("BP_SideScrollingCharacter")) Copy = CreateCore(CastChecked<UBlueprint>(Source), ARobotPlayerCharacter::StaticClass(), TEXT("BP_RobotPlayerCharacter"));
		else if (Name == TEXT("BP_SideScrollingCameraManager")) Copy = CreateCore(CastChecked<UBlueprint>(Source), ASideScrollingCameraManager::StaticClass(), Name);
		else if (Name == TEXT("BP_SideScrollingPlayerController")) Copy = CreateCore(CastChecked<UBlueprint>(Source), ASideScrollingPlayerController::StaticClass(), Name);
		else if (Name == TEXT("BP_SideScrollingGameMode")) Copy = CreateCore(CastChecked<UBlueprint>(Source), ASideScrollingGameMode::StaticClass(), Name);
		else
		{
			const FString Folder = Data.PackagePath.ToString().Replace(*SourceRoot, *TargetRoot);
			Copy = Tools.DuplicateAsset(Name, Folder, Source);
		}
		if (!Copy) return 1;
		Copies.Add(Copy);
		Replacements.Add(Source, Copy);
		if (UBlueprint* OriginalBP = Cast<UBlueprint>(Source))
		{
			UBlueprint* CopyBP = CastChecked<UBlueprint>(Copy);
			Replacements.Add(OriginalBP->GeneratedClass, CopyBP->GeneratedClass);
			Replacements.Add(OriginalBP->SkeletonGeneratedClass, CopyBP->SkeletonGeneratedClass);
			Replacements.Add(OriginalBP->GeneratedClass->GetDefaultObject(), CopyBP->GeneratedClass->GetDefaultObject());
			Core.Add(Name, CopyBP);
		}
	}

	// Rewrite object/class/soft references throughout each copied package, including map actors.
	for (UObject* Asset : Copies)
	{
		TArray<UObject*> Objects;
		GetObjectsWithPackage(Asset->GetOutermost(), Objects, EGetObjectsFlags::IncludeNestedObjects);
		for (UObject* Object : Objects)
		{
			FArchiveReplaceObjectRef<UObject> Archive(Object, Replacements,
				EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
		}
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			TArray<UEdGraph*> Graphs;
			Blueprint->GetAllGraphs(Graphs);
			for (UEdGraph* Graph : Graphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					// Class literals and soft-class checks are also stored as strings in graph pins.
					for (UEdGraphPin* Pin : Node->Pins)
					{
						for (const auto& Pair : Replacements)
						{
							if (Pair.Key && Pair.Value) Pin->DefaultValue.ReplaceInline(*Pair.Key->GetPathName(), *Pair.Value->GetPathName());
						}
					}
					if (UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
					{
						const FName Member = Variable->VariableReference.GetMemberName();
						if (Member == TEXT("Has Wall Jumped") || Member == TEXT("Has Double Jumped"))
						{
							const FName NativeMember = Member == TEXT("Has Wall Jumped") ? TEXT("bHasWallJumped") : TEXT("bHasDoubleJumped");
							for (UEdGraphPin* Pin : Variable->Pins)
							{
								if (Pin->PinName == Member) Pin->PinName = NativeMember;
							}
							Variable->VariableReference.SetExternalMember(NativeMember, ARobotPlayerCharacter::StaticClass());
							Variable->ReconstructNode();
						}
					}
				}
			}
		}
	}

	UBlueprint* Character = Core.FindChecked(TEXT("BP_SideScrollingCharacter"));
	UBlueprint* Controller = Core.FindChecked(TEXT("BP_SideScrollingPlayerController"));
	UBlueprint* Camera = Core.FindChecked(TEXT("BP_SideScrollingCameraManager"));
	UBlueprint* GameMode = Core.FindChecked(TEXT("BP_SideScrollingGameMode"));
	UClass* SideInterface = LoadObject<UBlueprint>(nullptr, *(SourceRoot / TEXT("Blueprints/BPI_SideScrolling.BPI_SideScrolling")))->GeneratedClass;
	UClass* TouchInterface = LoadObject<UBlueprint>(nullptr, *(SourceRoot / TEXT("Input/BPI_TouchInterface_SideScrolling.BPI_TouchInterface_SideScrolling")))->GeneratedClass;
	FBlueprintEditorUtils::ImplementNewInterface(Character, SideInterface->GetClassPathName());
	FBlueprintEditorUtils::ImplementNewInterface(Character, TouchInterface->GetClassPathName());
	FBlueprintEditorUtils::ImplementNewInterface(GameMode, SideInterface->GetClassPathName());
	AddBridge(Character, SideInterface, TEXT("SetSoftCollision"), ARobotPlayerCharacter::StaticClass(), TEXT("ApplySoftCollision"), 0);
	AddBridge(Character, SideInterface, TEXT("SoftCollisionSwitch"), ARobotPlayerCharacter::StaticClass(), TEXT("ResetSoftCollision"), 1);
	AddBridge(Character, TouchInterface, TEXT("Touch Jump Start"), ARobotPlayerCharacter::StaticClass(), TEXT("DoJumpStart"), 2);
	AddBridge(Character, TouchInterface, TEXT("Touch Jump End"), ARobotPlayerCharacter::StaticClass(), TEXT("DoJumpEnd"), 3);
	AddBridge(Character, TouchInterface, TEXT("Touch Interact Start"), ARobotPlayerCharacter::StaticClass(), TEXT("DoInteract"), 4);
	AddBridge(Character, TouchInterface, TEXT("Primary Thumbstick"), ARobotPlayerCharacter::StaticClass(), TEXT("DoTouchMove"), 5);
	AddBridge(GameMode, SideInterface, TEXT("Interaction"), ASideScrollingGameMode::StaticClass(), TEXT("ProcessPickup"), 0);

	// Compile interfaces first, then native children, then dependent animation/UI/gameplay assets.
	for (UObject* Asset : Copies)
	{
		if (UBlueprint* BP = Cast<UBlueprint>(Asset); BP && BP->BlueprintType == BPTYPE_Interface)
		{
			if (!Compile(BP)) return 1;
		}
	}
	for (UBlueprint* BP : {Character, Camera, Controller, GameMode})
	{
		if (!Compile(BP)) return 1;
	}
	for (UObject* Asset : Copies)
	{
		if (UBlueprint* BP = Cast<UBlueprint>(Asset))
		{
			if (BP != Character && BP != Camera && BP != Controller && BP != GameMode && BP->BlueprintType != BPTYPE_Interface)
			{
				FBlueprintEditorUtils::RefreshAllNodes(BP);
				if (!Compile(BP)) return 1;
			}
		}
	}

	ARobotPlayerCharacter* CharacterDefaults = CastChecked<ARobotPlayerCharacter>(Character->GeneratedClass->GetDefaultObject());
	const ACharacter* Original = CastChecked<ACharacter>(LoadObject<UBlueprint>(nullptr, *(SourceRoot / TEXT("Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter")))->GeneratedClass->GetDefaultObject());
	CharacterDefaults->GetMesh()->SetSkeletalMeshAsset(Original->GetMesh()->GetSkeletalMeshAsset());
	CharacterDefaults->GetMesh()->SetRelativeTransform(Original->GetMesh()->GetRelativeTransform());
	SetObject(CharacterDefaults->GetMesh(), TEXT("AnimClass"), Core.FindChecked(TEXT("ABP_Manny_SideScroller"))->GeneratedClass);
	const FString ThirdPerson = TEXT("/Game/_ExternalAssets/ThirdPerson");
	SetObject(CharacterDefaults, TEXT("MoveAction"), LoadObject<UObject>(nullptr, *(ThirdPerson / TEXT("Input/Actions/IA_Move.IA_Move"))));
	SetObject(CharacterDefaults, TEXT("JumpAction"), LoadObject<UObject>(nullptr, *(ThirdPerson / TEXT("Input/Actions/IA_Jump.IA_Jump"))));
	SetObject(CharacterDefaults, TEXT("DropAction"), LoadObject<UObject>(nullptr, *(TargetRoot / TEXT("Input/Actions/IA_Drop.IA_Drop"))));
	SetObject(CharacterDefaults, TEXT("InteractAction"), LoadObject<UObject>(nullptr, *(TargetRoot / TEXT("Input/Actions/IA_Interact.IA_Interact"))));
	UObject* ControllerDefaults = Controller->GeneratedClass->GetDefaultObject();
	SetObject(ControllerDefaults, TEXT("SideScrollingMappingContext"), LoadObject<UObject>(nullptr, *(TargetRoot / TEXT("Input/IMC_SideScroller.IMC_SideScroller"))));
	SetObject(ControllerDefaults, TEXT("MouseMappingContext"), LoadObject<UObject>(nullptr, *(ThirdPerson / TEXT("Input/IMC_MouseLook.IMC_MouseLook"))));
	SetObject(ControllerDefaults, TEXT("TouchControlsWidgetClass"), Core.FindChecked(TEXT("UI_TouchInterface_SideScrolling"))->GeneratedClass);
	SetObject(ControllerDefaults, TEXT("PlayerCameraManagerClass"), Camera->GeneratedClass);
	UObject* GameModeDefaults = GameMode->GeneratedClass->GetDefaultObject();
	SetObject(GameModeDefaults, TEXT("DefaultPawnClass"), Character->GeneratedClass);
	SetObject(GameModeDefaults, TEXT("PlayerControllerClass"), Controller->GeneratedClass);
	SetObject(GameModeDefaults, TEXT("PickupClass"), Core.FindChecked(TEXT("BP_PickUp"))->GeneratedClass);
	SetObject(GameModeDefaults, TEXT("UserInterfaceClass"), Core.FindChecked(TEXT("UI_PickupCount"))->GeneratedClass);
	if (SourceWorld) Copies.Add(CopyTemplateWorld(SourceWorld, Replacements));
	for (UObject* Asset : Copies)
	{
		if (UWorld* World = Cast<UWorld>(Asset)) World->GetWorldSettings()->DefaultGameMode = GameMode->GeneratedClass;
		if (!Save(Asset)) return 1;
	}
	UE_LOG(LogTemp, Display, TEXT("SIDESCROLLER_MIGRATION_OK: %d assets; native character, controller, camera and game mode; %s"), Copies.Num(), *TargetRoot);
	return 0;
}
