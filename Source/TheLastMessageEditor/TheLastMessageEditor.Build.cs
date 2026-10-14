using UnrealBuildTool;

public class TheLastMessageEditor : ModuleRules
{
	public TheLastMessageEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "TheLastMessage", "UnrealEd", "AssetTools",
			"AssetRegistry", "Kismet", "BlueprintGraph", "KismetCompiler", "EnhancedInput", "UMG"
		});
	}
}
