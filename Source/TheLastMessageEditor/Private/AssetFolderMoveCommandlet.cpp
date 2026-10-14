#include "AssetFolderMoveCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/Parse.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
bool FixGraphPaths(IAssetRegistry& Registry, const FString& Source, const FString& Destination)
{
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Destination), Assets, true);
	for (const FAssetData& Data : Assets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Data.GetAsset());
		if (!Blueprint) continue;
		bool bChanged = false;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					// Soft-class literals can remain as graph strings after an asset rename.
					if (Pin->DefaultValue.Contains(Source + TEXT("/")))
					{
						Graph->GetSchema()->TrySetDefaultValue(*Pin, Pin->DefaultValue.Replace(*Source, *Destination), false);
						bChanged = true;
					}
				}
			}
		}
		if (!bChanged) continue;
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		if (Results.NumErrors > 0) return false;
		FSavePackageArgs Save;
		Save.TopLevelFlags = RF_Public | RF_Standalone;
		const FString File = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *File, Save)) return false;
		UE_LOG(LogTemp, Display, TEXT("Updated graph package paths: %s"), *Blueprint->GetPathName());
	}
	return true;
}
}

int32 UAssetFolderMoveCommandlet::Main(const FString& Params)
{
	FString Source, Destination;
	if (!FParse::Value(*Params, TEXT("Source="), Source) ||
		!FParse::Value(*Params, TEXT("Destination="), Destination) ||
		!Source.StartsWith(TEXT("/Game/")) || !Destination.StartsWith(TEXT("/Game/Test_Jihong/")) ||
		Source == Destination)
	{
		UE_LOG(LogTemp, Error, TEXT("Specify Source=/Game/... and Destination=/Game/Test_Jihong/..."));
		return 1;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	if (Params.Contains(TEXT("FixReferences"))) return FixGraphPaths(Registry, Source, Destination) ? 0 : 1;
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Source), Assets, true);
	UE_LOG(LogTemp, Display, TEXT("Asset folder move: %d assets in %s"), Assets.Num(), *Source);
	if (Assets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Source folder contains no registered assets."));
		return 1;
	}
	TArray<FAssetData> Existing;
	Registry.GetAssetsByPath(FName(*Destination), Existing, true);
	if (!Existing.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Destination already contains assets; refusing to overwrite."));
		return 1;
	}
	TArray<FAssetRenameData> Moves;
	// Load all assets before moving so cross-references and the level are updated together.
	for (const FAssetData& Data : Assets)
	{
		UObject* Asset = Data.GetAsset();
		if (!Asset || Asset->IsA<UObjectRedirector>())
		{
			UE_LOG(LogTemp, Error, TEXT("Cannot move asset %s: missing or already a redirector."), *Data.GetSoftObjectPath().ToString());
			return 1;
		}
		const FString Folder = Destination + Data.PackagePath.ToString().RightChop(Source.Len());
		Moves.Emplace(Asset, Folder, Data.AssetName.ToString());
	}
	IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	if (!Tools.RenameAssets(Moves))
	{
		UE_LOG(LogTemp, Error, TEXT("Unreal's asset rename operation did not complete."));
		return 1;
	}
	if (!FixGraphPaths(Registry, Source, Destination)) return 1;
	TArray<UObjectRedirector*> Redirectors;
	for (const FAssetData& Data : Assets)
	{
		if (UObjectRedirector* Redirector = FindObject<UObjectRedirector>(nullptr, *Data.GetSoftObjectPath().ToString()))
		{
			Redirectors.Add(Redirector);
		}
	}
	if (!Redirectors.IsEmpty()) Tools.FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);
	UE_LOG(LogTemp, Display, TEXT("ASSET_FOLDER_MOVE_OK: %d assets moved to %s, %d redirectors fixed."), Moves.Num(), *Destination, Redirectors.Num());
	return 0;
}
