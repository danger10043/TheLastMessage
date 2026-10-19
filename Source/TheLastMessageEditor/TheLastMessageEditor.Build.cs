using UnrealBuildTool;

public class TheLastMessageEditor : ModuleRules
{
    public TheLastMessageEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "TheLastMessage", "UnrealEd", "UMG", "UMGEditor",
            "Slate", "SlateCore", "SlateRHIRenderer", "KismetCompiler", "AssetRegistry", "RenderCore", "RHI", "ImageWrapper"
        });
    }
}
