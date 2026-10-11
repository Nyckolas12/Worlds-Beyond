// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

// Editor-only tools for building the open world (Plan 5B): the heightfield generator, the World Partition landscape,
// its material and layers, the painted world map. Driven by the migration scripts (migrate_pass15/16.py).
public class WorldBeyondEditor : ModuleRules
{
	public WorldBeyondEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"WorldBeyond",
			"UnrealEd",
			"Landscape",
			"AssetTools",
			"AssetRegistry",
			"MaterialEditor",
			"NavigationSystem",
			"ImageWrapper",
			"RenderCore",
			"RHI"
		});
	}
}
