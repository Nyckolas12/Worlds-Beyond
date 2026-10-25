// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondGroundLibrary.generated.h"

class ULandscapeGrassType;
class UBeyondWorldLayout;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UStaticMesh;
class UTexture;
class UTexture2D;

/** Where a value comes from in a packed texture (None: the look's constant) */
UENUM(BlueprintType)
enum class EBeyondGroundChannel : uint8
{
	None,
	R,
	G,
	B,
	A
};

/** A glow a ground layer can have */
UENUM(BlueprintType)
enum class EBeyondGroundGlow : uint8
{
	None,
	// Bright specks: the mask's red raised to Glow Sharpness (ash with embers)
	Embers,
	// Thin lines where the mask's red crosses the middle (corrupted ground)
	Veins
};

/** How the frost region changes a layer */
UENUM(BlueprintType)
enum class EBeyondGroundFrostVariant : uint8
{
	None,
	// The trampled snow set replaces the layer (paths, dirt yards)
	TrampledSnow,
	// Snow settles where the layer is low (between cobbles)
	SnowInGaps,
	// Snow lies on the faces that point up (rock)
	SnowOnTop
};

/** One ground layer's look in the landscape material (Plan 5D); every value becomes a parameter of the material */
USTRUCT(BlueprintType)
struct WORLDBEYONDEDITOR_API FBeyondGroundLayerLook
{
	GENERATED_BODY()

	// Landscape layer name (BeyondTerrainLayers)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName Layer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> BaseColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> Normal;

	// Roughness map (or a packed map: see Roughness Channel); none: Roughness
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> RoughnessMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	EBeyondGroundChannel RoughnessChannel = EBeyondGroundChannel::R;

	// Height map for the height blend; none: the base colour's brightness
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> HeightMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	EBeyondGroundChannel HeightChannel = EBeyondGroundChannel::R;

	// Metres one repeat of the textures covers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float TileSize = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float Saturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float Brightness = 1.0f;

	// Without a roughness map; else the map is multiplied by Roughness Scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float Roughness = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float RoughnessScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float NormalStrength = 1.0f;

	// The height blend: higher contrast gives crisper edges where layers meet; the offset pushes a layer over the others
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float HeightContrast = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float HeightOffset = 0.0f;

	// Projected from three sides (cliffs don't stretch)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	bool bTriplanar = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	EBeyondGroundFrostVariant FrostVariant = EBeyondGroundFrostVariant::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FrostAmount = 1.0f;

	// In each region the layer can turn into another layer's look (dirt under snow in the Rimewood, charred in the
	// Cinderlands, corrupted in the Blightwood): that layer, and how much of it where the region is full
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName FrostLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FrostLayerAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName MoltenLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float MoltenLayerAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName BlightLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float BlightLayerAmount = 0.0f;

	// Patches of another layer scattered through this one in the Elderwood (grass growing through a village's dirt)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName PatchLayer;

	// Share of the ground the patches cover (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float PatchAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float PatchTileSize = 18.0f;

	// Higher: harder patch edges
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float PatchSharpness = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	EBeyondGroundGlow Glow = EBeyondGroundGlow::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> GlowMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float GlowTileSize = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FLinearColor GlowColour = FLinearColor(1.0f, 0.35f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float GlowStrength = 1.0f;

	// Embers: the exponent (higher, fewer specks); veins: the line width (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float GlowSharpness = 6.0f;
};

/** A region's colour grade over the blended ground */
USTRUCT(BlueprintType)
struct WORLDBEYONDEDITOR_API FBeyondGroundRegionGrade
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float Saturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float Brightness = 1.0f;
};

/** What the whole ground shares: the region mask and grades, the frost's snow, tiling and variation */
USTRUCT(BlueprintType)
struct WORLDBEYONDEDITOR_API FBeyondGroundSettings
{
	GENERATED_BODY()

	// T_Beyond_RegionMask (red frost, green molten, blue blight), laid over the landscape
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> RegionMask;

	// Metres the region mask spans (the landscape's width)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float RegionMaskSize = 4032.0f;

	// The Elderwood (wherever none of the other three is)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FBeyondGroundRegionGrade Forest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FBeyondGroundRegionGrade Frost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FBeyondGroundRegionGrade Molten;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FBeyondGroundRegionGrade Blight;

	// The frost region's paths (Frost Variant = Trampled Snow)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> TrampledSnowBaseColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> TrampledSnowNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> TrampledSnowRoughness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	EBeyondGroundChannel TrampledSnowRoughnessChannel = EBeyondGroundChannel::G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float TrampledSnowTileSize = 3.0f;

	// The layer whose look is the snow on rock tops and in gaps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName SnowLayer = TEXT("Snow");

	// Up-facing amount (world normal Z) where snow starts / covers rock
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float RockSnowStart = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float RockSnowEnd = 0.88f;

	// Height below which snow fills a Snow In Gaps layer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float GapSnowLevel = 0.42f;

	// Large-scale brightness variation that breaks up repeats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UTexture> MacroNoise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float MacroTileSize = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float MacroTileSize2 = 230.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float MacroStrength = 0.18f;

	// The ground drifts between these two tints over the larger macro scale (drier and lusher stretches)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FLinearColor VariationTintA = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FLinearColor VariationTintB = FLinearColor::White;

	// Far away the base colour is also sampled this much larger (and turned), so repeats don't line up into a grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FarTileScale = 0.23f;

	// How much of the far sample is mixed in once fully far (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FarMix = 0.5f;

	// Distance (cm) the far sampling fades in over
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FarBlendStart = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FarBlendEnd = 6000.0f;

	// How much the normals flatten far away (0 keeps them)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float FarNormalFade = 0.5f;

	// How much glows fade far away (thin veins and specks shimmer at a distance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float GlowFarFade = 0.85f;
};

/** A landscape grass type grown where a layer is painted */
USTRUCT(BlueprintType)
struct WORLDBEYONDEDITOR_API FBeyondGroundGrassInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<ULandscapeGrassType> GrassType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FName Layer;

	// How much grows in each region: R the Elderwood, G the Rimewood, B the Cinderlands, A the Blightwood
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FLinearColor Regions = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};

/** One mesh of a grass type */
USTRUCT(BlueprintType)
struct WORLDBEYONDEDITOR_API FBeyondGrassVarietySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UStaticMesh> Mesh;

	// Instances per 10 x 10 m where the layer is fully painted
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float Density = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float ScaleMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	float ScaleMax = 1.2f;

	// cm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	int32 StartCullDistance = 3000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	int32 EndCullDistance = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	bool bAlignToSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

/**
 * The open world's ground (Plan 5D), called by migrate_pass18.py: the textured landscape material (a height blend of the
 * 11 ground layers, each a set of texture parameters; a region mask grades the Blightwood, the Rimewood and the
 * Cinderlands and puts snow on the frost's paths and rocks), its instance, the region mask, landscape grass types,
 * putting a material on a partitioned landscape, and growing grass for a headless picture. Assets are made in memory;
 * the script saves them.
 */
UCLASS()
class WORLDBEYONDEDITOR_API UBeyondGroundLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// The landscape's layer names, in order
	UFUNCTION(BlueprintPure, Category = "Beyond|Ground")
	static TArray<FName> GetLayerNames();

	// Deletes every node of a material, outputs included (for rebuilding one from a script)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground")
	static void ClearMaterialGraph(UMaterial* Material);

	/**
	 * The textured landscape material: Looks (one per layer; a layer without one keeps its greybox colour), Settings, the
	 * grass grown on layers. An existing one is returned untouched unless bRebuild.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground")
	static UMaterial* CreateGroundMaterial(const FString& AssetPath, const TArray<FBeyondGroundLayerLook>& Looks, const FBeyondGroundSettings& Settings,
		const TArray<FBeyondGroundGrassInput>& Grass, bool bRebuild = false);

	// An instance of Parent (only made when missing; bReset clears its overrides)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground")
	static UMaterialInstanceConstant* CreateGroundMaterialInstance(const FString& AssetPath, UMaterialInterface* Parent, bool bReset = false);

	// T_Beyond_RegionMask: red frost, green molten, blue blight (the layout's region weights), north row first
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground")
	static UTexture2D* BakeRegionMask(UBeyondWorldLayout* Layout, const FString& AssetPath, int32 Resolution = 512);

	// A landscape grass type with these meshes (made or rewritten)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground")
	static ULandscapeGrassType* CreateGrassType(const FString& AssetPath, const TArray<FBeyondGrassVarietySpec>& Varieties);

	// The material on the world's landscape (none without one)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground", meta = (WorldContext = "WorldContext"))
	static UMaterialInterface* GetLandscapeMaterial(UObject* WorldContext);

	/**
	 * Puts Material on the landscape and every streaming proxy (all are loaded for it; the script saves the world).
	 * Returns how many proxies carry it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground", meta = (WorldContext = "WorldContext"))
	static int32 SetLandscapeMaterial(UObject* WorldContext, UMaterialInterface* Material);

	/**
	 * Grows the landscape grass around Cameras now (builds the grass maps first). Dirties the landscape: use it after
	 * saving, for pictures. Returns the grass instances there are afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Ground", meta = (WorldContext = "WorldContext"))
	static int32 GrowGrassAt(UObject* WorldContext, const TArray<FVector>& Cameras);
};
