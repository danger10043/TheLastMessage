// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class TheLastMessage : ModuleRules
{
    public TheLastMessage(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
                                                            "GameplayAbilities", "GameplayTags", "GameplayTasks",
                                                            "AIModule", "NavigationSystem","Niagara", "EnhancedInput",
                                                            "StateTreeModule", "GameplayStateTreeModule", "UMG", "Slate"});

        // UI 입력 처리에 쓰는 FReply와 화면 배치 정보인 FGeometry 등 Slate 기본 타입을 사용한다.
        PrivateDependencyModuleNames.AddRange(new string[] { "MoviePlayer", "SlateCore", "CableComponent", "LevelSequence", "MovieScene", "ProceduralMeshComponent" });
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMGEditor", "AssetRegistry", "RenderCore", "RHI", "SlateRHIRenderer", "NiagaraEditor" });
        }

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
