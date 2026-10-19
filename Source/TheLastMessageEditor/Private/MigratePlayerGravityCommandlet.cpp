#include "MigratePlayerGravityCommandlet.h"
#include "Character/Player/PlayerBase.h"
#include "Gimmick/Component/GravityStateComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

UMigratePlayerGravityCommandlet::UMigratePlayerGravityCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UMigratePlayerGravityCommandlet::Main(const FString& Params)
{
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Test_Jihong/BP_TestPlayer.BP_TestPlayer"));
	if (!BP || !BP->SimpleConstructionScript || !BP->GeneratedClass->IsChildOf(APlayerBase::StaticClass())) return 1;
	const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	const FString Backup = FPaths::ProjectSavedDir() / TEXT("GravityMigration/BP_TestPlayer.uasset");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
	TArray<USCS_Node*> OldNodes;
	for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UGravityStateComponent>()) OldNodes.Add(Node);
	}
	if (OldNodes.Num() > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("GravityMigration: multiple temporary components need manual reconciliation."));
		return 1;
	}
	if (OldNodes.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("GravityMigration: already uses the native component."));
		return 0;
	}
	if (IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK) return 1;
	TStrongObjectPtr<UGravityStateComponent> Previous(DuplicateObject<UGravityStateComponent>(
		CastChecked<UGravityStateComponent>(OldNodes[0]->ComponentTemplate), GetTransientPackage()));
	for (USCS_Node* Node : OldNodes)
	{
		FBlueprintEditorUtils::ReplaceVariableReferences(BP, Node->GetVariableName(), TEXT("GravityStateComponent"));
		BP->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Node);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) return 1;
	UGravityStateComponent* Native = BP->GeneratedClass->GetDefaultObject<APlayerBase>()->FindComponentByClass<UGravityStateComponent>();
	if (!Native || Native->CreationMethod != EComponentCreationMethod::Native) return 1;
	// Preserve the temporary component's authored GE and transition duration.
	for (const FName Name : {FName(TEXT("GravityEffectClass")), FName(TEXT("GravityRotationDuration"))})
	{
		if (FProperty* Property = FindFProperty<FProperty>(UGravityStateComponent::StaticClass(), Name))
			Property->CopyCompleteValue_InContainer(Native, Previous.Get());
	}
	BP->MarkPackageDirty();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, SaveArgs)) return 1;
	UE_LOG(LogTemp, Display, TEXT("GRAVITY_MIGRATED: BP_TestPlayer now inherits GravityStateComponent; temporary component removed; settings preserved."));
	return 0;
}
