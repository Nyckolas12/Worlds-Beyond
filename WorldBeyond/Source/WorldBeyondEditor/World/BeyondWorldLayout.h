// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BeyondWorldLayout.generated.h"

// How a region's ground rises and falls
UENUM(BlueprintType)
enum class EBeyondTerrainStyle : uint8
{
	// Rolling hills, forest floor and grass
	Forest,
	// Low, boggy, dark soil
	Blight,
	// Ridged, snowy, higher
	Frost,
	// Ridged, ash and rock
	Molten
};

// What covers a flattened pad
UENUM(BlueprintType)
enum class EBeyondPadSurface : uint8
{
	Dirt,
	Cobble,
	Rock,
	// The region's own ground
	Ground
};

USTRUCT(BlueprintType)
struct FBeyondLayoutRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	FName RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	EBeyondTerrainStyle Style = EBeyondTerrainStyle::Forest;

	// Metres east / north of the world's centre, in order
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	TArray<FVector2D> Polygon;

	// Height of the ground (m) before hills
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	float BaseHeight = 30.0f;

	// Hills rise / fall this much (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	float HillHeight = 25.0f;

	// Width of a hill (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = "20"))
	float HillScale = 320.0f;

	// Colour on the painted map
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	FLinearColor MapColour = FLinearColor(0.4f, 0.6f, 0.3f);
};

// A flattened area: a village, an arena, a waystone, a camp
USTRUCT(BlueprintType)
struct FBeyondLayoutPad
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad")
	FVector2D Centre = FVector2D::ZeroVector;

	// Flat inside this radius (m)...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad", meta = (ClampMin = "1"))
	float Radius = 30.0f;

	// ...blending back to the ground over this much more (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad", meta = (ClampMin = "1"))
	float Falloff = 30.0f;

	// Raised / lowered from the ground's average under it (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad")
	float HeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pad")
	EBeyondPadSurface Surface = EBeyondPadSurface::Dirt;
};

USTRUCT(BlueprintType)
struct FBeyondLayoutRoad
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FName Id;

	// Metres east / north
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TArray<FVector2D> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road", meta = (ClampMin = "1"))
	float Width = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	bool bCobble = false;
};

// A long rise between regions; passes cut through it
USTRUCT(BlueprintType)
struct FBeyondLayoutRidge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
	TArray<FVector2D> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
	float Height = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge", meta = (ClampMin = "10"))
	float Width = 160.0f;

	// Gaps through the ridge (centres, m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
	TArray<FVector2D> Passes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge", meta = (ClampMin = "10"))
	float PassWidth = 110.0f;
};

USTRUCT(BlueprintType)
struct FBeyondLayoutPeak
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak")
	FVector2D Centre = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak", meta = (ClampMin = "10"))
	float Radius = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak")
	float Height = 250.0f;

	// Higher: steeper near the top
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak", meta = (ClampMin = "0.5"))
	float Sharpness = 1.6f;
};

USTRUCT(BlueprintType)
struct FBeyondLayoutLake
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lake")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lake")
	FVector2D Centre = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lake", meta = (ClampMin = "10"))
	float Radius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lake", meta = (ClampMin = "0.5"))
	float Depth = 8.0f;

	// Ice you can walk on
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lake")
	bool bFrozen = false;
};

USTRUCT(BlueprintType)
struct FBeyondLayoutVolcano
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	FVector2D Centre = FVector2D::ZeroVector;

	// The cone's foot (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float Radius = 550.0f;

	// The crater's rim (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float RimRadius = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float RimHeight = 260.0f;

	// The crater's floor (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float FloorHeight = 140.0f;

	// Lava pools below this (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float LavaLevel = 134.0f;

	// The rim is broken toward this compass bearing (degrees: 0 north, 90 east), a way in
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float BreachBearing = 135.0f;

	// Half the breach's opening angle (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano")
	float BreachHalfAngle = 14.0f;
};

/**
 * The open world's shape (Plan 5B), read by the world builder to make the landscape: regions with their terrain style,
 * ridges and their passes, peaks, a volcano, lakes, roads and flat pads for villages, arenas, waystones and camps.
 * Positions in metres east (X) / north (Y) of the world's centre. Seeded by migrate_pass15.py from world_content.py
 * (DA_WorldLayout); edit it and rebuild the terrain with BEYOND_REBUILD_TERRAIN=1 (that loses hand sculpting).
 */
UCLASS(BlueprintType)
class WORLDBEYONDEDITOR_API UBeyondWorldLayout : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	int32 Seed = 1977;

	// Landscape components per side (each 2 x 2 sections of 63 quads at 1 m: 32 -> 4033 x 4033 vertices, 4 km)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "2", ClampMax = "64"))
	int32 ComponentsPerSide = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "7", ClampMax = "255"))
	int32 QuadsPerSection = 63;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "1", ClampMax = "2"))
	int32 SectionsPerComponent = 2;

	// Landscape Z scale (200: heights from -512 m to +512 m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	float HeightScaleZ = 200.0f;

	// Mountains rise along the world's edge over this width (m)...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	float BorderWidth = 220.0f;

	// ...to this height (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	float BorderHeight = 300.0f;

	// Regions blend into each other over this width (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	float RegionBlend = 180.0f;

	// Region borders and ridges wander this far (m) from their straight lines, so they read as land, not a grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World")
	float BorderWarp = 90.0f;

	// Length of one wander (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World", meta = (ClampMin = "50"))
	float BorderWarpScale = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<FBeyondLayoutRegion> Regions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<FBeyondLayoutRidge> Ridges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<FBeyondLayoutPeak> Peaks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FBeyondLayoutVolcano Volcano;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<FBeyondLayoutLake> Lakes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<FBeyondLayoutRoad> Roads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	TArray<FBeyondLayoutPad> Pads;

	// Vertices per side
	int32 GetSize() const { return ComponentsPerSide * SectionsPerComponent * QuadsPerSection + 1; }

	// Metres from the centre to the edge
	float GetHalfExtent() const { return (GetSize() - 1) * 0.5f; }
};
