// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondWorldBuilderLibrary.generated.h"

class ALandscape;
class UBeyondWorldLayout;
class UBeyondWorldMapData;
class ULandscapeLayerInfoObject;
class UMaterial;
class UMaterialInterface;

/**
 * The world builder (Plan 5B), called by migrate_pass15.py / migrate_pass16.py: the terrain from a layout, the World
 * Partition landscape with its layers and greybox material, ground heights for placing things, the painted world map,
 * the open world's navigation set-up, saving a partitioned map. Assets are made in memory; the scripts save them.
 * The generated heightfield is kept between calls for the same layout (one generation per pass).
 */
UCLASS()
class WORLDBEYONDEDITOR_API UBeyondWorldBuilderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Generates the terrain and writes Saved/WorldBuilder/ (heightmap.r16, heightmap.png, preview.png); returns the folder
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static FString WriteTerrainPreview(UBeyondWorldLayout* Layout);

	// The greybox landscape material: one flat colour per ground layer (an existing one is returned unless bRebuild)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static UMaterial* CreateGreyboxLandscapeMaterial(const FString& AssetPath, bool bRebuild = false);

	// A layer info per ground layer in Folder (LI_<Layer>; existing ones are reused)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static TArray<ULandscapeLayerInfoObject*> CreateLayerInfos(const FString& Folder);

	/**
	 * The landscape from the layout: heights and layer weights imported into an edit layer named "Generated", an empty
	 * "Sculpt" layer above it for hand work, split into streaming proxies of Grid Size components in a partitioned world.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static ALandscape* CreateLandscape(UObject* WorldContext, UBeyondWorldLayout* Layout, UMaterialInterface* Material,
		const TArray<ULandscapeLayerInfoObject*>& LayerInfos, int32 GridSizeInComponents = 2);

	// Deletes actors so their files really go when the world is saved (a partitioned map keeps one file per actor)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static int32 DestroyActors(const TArray<AActor*>& Actors);

	// Deletes every landscape and landscape proxy in the world; returns how many
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static int32 DeleteLandscapes(UObject* WorldContext);

	// Loads every actor of a partitioned world whose label starts with Prefix (they stay loaded until the world is saved)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static int32 LoadActorsWithLabelPrefix(UObject* WorldContext, const FString& Prefix);

	// Loads every landscape proxy of a partitioned world (to read heights / edit); returns how many are loaded
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static int32 LoadLandscapeProxies(UObject* WorldContext);

	// The ground's height (Unreal Z) at an Unreal X / Y: the landscape when it is loaded there, else the layout's terrain
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static bool GetGroundHeight(UObject* WorldContext, UBeyondWorldLayout* Layout, float X, float Y, float& OutZ);

	// The layout's terrain height (Unreal Z) at an Unreal X / Y (what the landscape was generated from)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static float GetLayoutHeight(UBeyondWorldLayout* Layout, float X, float Y);

	// Water level (Unreal Z) of a layout lake
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static bool GetLakeLevel(UBeyondWorldLayout* Layout, FName LakeId, float& OutZ);

	/**
	 * The painted world map from the terrain: T_WorldMap (Resolution px square: ground colours by layer, region tint,
	 * hillshade, contours, water, roads, forest stipple, paper), a fog mask per region and DA_WorldMap pointing at them,
	 * all in Folder. Existing assets are updated in place.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static UBeyondWorldMapData* BakeWorldMap(UBeyondWorldLayout* Layout, const FString& Folder, int32 Resolution = 2048);

	/**
	 * Navigation for the open world: World Settings -> Navigation System Class = BeyondOpenWorldNavigationSystem and a
	 * RecastNavMesh with dynamic runtime generation (always loaded). Returns false without a world.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static bool ConfigureOpenWorld(UObject* WorldContext);

	// Saves the map and every dirty actor package of a partitioned world
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder", meta = (WorldContext = "WorldContext"))
	static bool SaveWorld(UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static void SetSpatiallyLoaded(AActor* Actor, bool bSpatiallyLoaded);

	// Forget the generated heightfield (the next call generates it again)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World Builder")
	static void ClearTerrainCache();
};
