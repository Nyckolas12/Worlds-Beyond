// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class WorldBeyond : ModuleRules
{
	public WorldBeyond(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files include each other by module-relative path, e.g. "AI/BeyondCompanionController.h"
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "AIModule" });

		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem", "UMG", "Slate", "SlateCore", "Niagara" });

		// Automation tests (Source/WorldBeyond/Tests) drive Play In Editor
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "LevelSequence", "MovieScene" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
