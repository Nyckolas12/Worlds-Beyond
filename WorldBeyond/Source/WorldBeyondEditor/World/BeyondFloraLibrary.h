// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondFloraLibrary.generated.h"

class AActor;
class UBeyondWorldLayout;
class UFoliageType;
class UFoliageType_InstancedStaticMesh;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UTransformProviderData;

// What a planting is (the order things claim the ground: cliffs, trees, rocks, shrubs)
UENUM(BlueprintType)
enum class EBeyondFloraKind : uint8
{
	Cliff,
	Tree,
	Rock,
	Shrub
};

// One mesh of a planting and how often it's picked
USTRUCT(BlueprintType)
struct FBeyondFloraMesh
{
	GENERATED_BODY()

	// A static mesh (it goes into the foliage) or a skeletal Megaplant (into a flora tile)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	TObjectPtr<UObject> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora", meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/**
 * One kind of thing scattered over the open world (Plan 5E, from flora_content.py): where it grows (regions, layers,
 * slope, the grove noise, near water or not), how thick (per hectare), how big (a height in metres; the scale comes from
 * each mesh's bounds) and how much ground it keeps to itself.
 */
USTRUCT(BlueprintType)
struct FBeyondFloraPlanting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	EBeyondFloraKind Kind = EBeyondFloraKind::Tree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	TArray<FBeyondFloraMesh> Meshes;

	// Share in each region: R the Elderwood (Forest), G the Rimewood (Frost), B the Cinderlands (Molten), A the Blightwood
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	FLinearColor Regions = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);

	// Grows where these layers together weigh at least MinLayerWeight (empty: any ground)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	TArray<FName> Layers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (ClampMin = "0", ClampMax = "1"))
	float MinLayerWeight = 0.4f;

	// Never where these layers together weigh more than MaxAvoidWeight (roads, yards)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	TArray<FName> AvoidLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxAvoidWeight = 0.3f;

	// Ground slope (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	float MinSlope = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	float MaxSlope = 30.0f;

	// Where on the grove noise (as shares of the world, 0..1): 0..0.55 is the thickest 55 %, 0.55..0.65 the groves' edges
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (ClampMin = "0", ClampMax = "1"))
	float GroveMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (ClampMin = "0", ClampMax = "1"))
	float GroveMax = 1.0f;

	// Only on lake shores: the ground between these heights (m) relative to the nearest lake's water
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	bool bShore = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (EditCondition = "bShore"))
	float ShoreMin = -0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (EditCondition = "bShore"))
	float ShoreMax = 1.2f;

	// Only these lakes (empty: any); shores only
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where", meta = (EditCondition = "bShore"))
	TArray<FName> Lakes;

	// Ground height (m) limits (0, 0: any)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	FVector2D Altitude = FVector2D::ZeroVector;

	// Per hectare where everything above allows it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "How many", meta = (ClampMin = "0"))
	float Density = 10.0f;

	// Height (m) of a placed mesh, min / max (0, 0: use Scale)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	FVector2D Size = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	FVector2D Scale = FVector2D(1.0f, 1.0f);

	// Pushed into the ground: this share of the placed height plus SinkMetres
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	float Sink = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	float SinkMetres = 0.05f;

	// Ground kept free around it (m); 0: from the mesh's width
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	float Footprint = 0.0f;

	// Tilted with the ground (0 upright, 1 fully)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (ClampMin = "0", ClampMax = "1"))
	float AlignToSlope = 0.0f;

	// A random lean (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	float MaxTilt = 0.0f;

	// Turned to face down the slope (+ YawOffset) instead of a random yaw
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	bool bFaceDownhill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	float YawOffset = 0.0f;

	// Extra distance (m) from a road's edge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Where")
	float RoadClearance = 4.0f;
};

// A circle kept clear (a village, a waystone, a camp, an arena), metres east / north
USTRUCT(BlueprintType)
struct FBeyondFloraExclusion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	FVector2D Centre = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float Radius = 20.0f;

	// Shrubs may grow inside (heather at a camp's edge); only cliffs, trees and rocks are kept out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	bool bShrubsAllowed = false;
};

// Where one mesh of a planting went (world transforms)
USTRUCT(BlueprintType)
struct FBeyondFloraBatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Flora")
	FName Planting;

	UPROPERTY(BlueprintReadOnly, Category = "Flora")
	int32 MeshIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Flora")
	TObjectPtr<UObject> Mesh;

	UPROPERTY(BlueprintReadOnly, Category = "Flora")
	TArray<FTransform> Transforms;

	// How many in each region (Forest, Frost, Molten, Blight)
	UPROPERTY(BlueprintReadOnly, Category = "Flora")
	TArray<int32> RegionCounts;
};

// How a foliage type draws and blocks
USTRUCT(BlueprintType)
struct FBeyondFoliageSettings
{
	GENERATED_BODY()

	// Metres
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float CullStart = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float CullEnd = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	bool bCastShadow = true;

	// BlockAll and carves the navmesh; otherwise no collision at all
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	bool bBlocks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;

	// Brush defaults for painting the type by hand in Foliage mode: per 10 x 10 m, scale, sunk (m), slope limit, tilted
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting")
	float PaintDensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting")
	FVector2D PaintScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting")
	float PaintSink = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting")
	float PaintMaxSlope = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting")
	bool bPaintAlignToNormal = false;
};

// How a Megaplant draws in its tiles
USTRUCT(BlueprintType)
struct FBeyondSkinnedFloraSettings
{
	GENERATED_BODY()

	// Metres
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float CullStart = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float CullEnd = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	bool bCastShadow = true;

	// Sways it (the Procedural Vegetation plugin's dynamic wind)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	TObjectPtr<UTransformProviderData> Wind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;

	// A trunk collider per plant, radius / height in metres at the plant's own scale 1 (0: none)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float TrunkRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float TrunkHeight = 0.0f;

	// Tiles are this many metres square, labelled <LabelPrefix><x>_<y>
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	float TileSize = 256.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	FString LabelPrefix = TEXT("WB_Flora_");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flora")
	FName RuntimeGrid;
};

/**
 * Trees, shrubs and rocks for the open world (Plan 5E, migrate_pass19.py): scatters plantings over the generated terrain
 * (its layers, slopes, roads, lakes and regions) clear of the gameplay sites, makes foliage types for the static meshes,
 * fills flora tiles with the skeletal Megaplants, clears what an earlier run placed, and gives the flora its own World
 * Partition streaming grid.
 */
UCLASS()
class WORLDBEYONDEDITOR_API UBeyondFloraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Where every planting goes, in order (cliffs, trees, rocks, shrubs claim the ground first): one batch per mesh. Only
	// in OnlyRegions (Forest / Frost / Molten / Blight; empty: everywhere). Heights from the loaded landscape (else the
	// generated terrain). GroveScale: width (m) of the groves and clearings
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static TArray<FBeyondFloraBatch> ComputeFlora(UObject* WorldContext, UBeyondWorldLayout* Layout, const TArray<FBeyondFloraPlanting>& Plantings,
		const TArray<FBeyondFloraExclusion>& Exclusions, int32 Seed, const TArray<FName>& OnlyRegions, float GroveScale = 220.0f);

	// A dynamic wind transform provider asset (the Megaplants' sway), made when missing
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static UTransformProviderData* CreateWindData(const FString& AssetPath);

	// Height (m) of a mesh's bounds (static or skeletal)
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static float GetMeshHeight(UObject* Mesh);

	// The region (Forest / Frost / Molten / Blight) that weighs most at a point (Unreal cm)
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static FName GetRegionAt(UBeyondWorldLayout* Layout, float X, float Y);

	// A foliage type for a static mesh (made or updated)
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static UFoliageType_InstancedStaticMesh* CreateFoliageType(const FString& AssetPath, UStaticMesh* Mesh, const FBeyondFoliageSettings& Settings);

	// Replaces a tree's simple collision with one upright capsule round its trunk (metres, from the pivot up)
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static bool SetTrunkCollision(UStaticMesh* Mesh, float Radius, float Height);

	// Collision from the mesh's own triangles (cliffs, landmarks: walkable, no hull bridging their hollows)
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static bool UseComplexAsSimple(UStaticMesh* Mesh);

	// Simple collision shapes (boxes, spheres, capsules, hulls) on a mesh
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora")
	static int32 GetSimpleCollisionCount(UStaticMesh* Mesh);

	// Adds a Megaplant's instances to the flora tiles they fall in (made as needed); returns the tiles touched
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static int32 PlaceSkinnedFlora(UObject* WorldContext, USkeletalMesh* Mesh, const TArray<FTransform>& Transforms, const FBeyondSkinnedFloraSettings& Settings);

	// Loads every foliage actor and flora tile of a partitioned world (kept loaded until SaveWorld)
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static int32 LoadFloraActors(UObject* WorldContext);

	// Instances of these foliage types plus the flora tiles' plants, everywhere or in OnlyRegions; returns how many went
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static int32 ClearFlora(UObject* WorldContext, UBeyondWorldLayout* Layout, const TArray<UFoliageType*>& FoliageTypes, const TArray<FName>& OnlyRegions);

	// Instances of a foliage type in the loaded world
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static int32 CountFoliageInstances(UObject* WorldContext, UFoliageType* FoliageType);

	// Adds (or updates) a runtime partition of the world's RuntimeHashSet: cells and loading range in metres
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static bool ConfigureFloraStreaming(UObject* WorldContext, FName PartitionName, float CellSize, float LoadingRange);

	// Puts the loaded foliage actors and flora tiles on a runtime grid; returns how many changed
	UFUNCTION(BlueprintCallable, Category = "Worlds Beyond|Flora", meta = (WorldContext = "WorldContext"))
	static int32 AssignFloraGrid(UObject* WorldContext, FName PartitionName);
};
