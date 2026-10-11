// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UBeyondWorldLayout;

/** The landscape's ground layers, in the landscape material's order (Plan 5B) */
namespace BeyondTerrainLayers
{
	enum ELayer : int32
	{
		ForestGround,
		Moss,
		Grass,
		Dirt,
		Path,
		Cobble,
		Mud,
		Rock,
		Snow,
		Ash,
		Corrupt,
		Count
	};

	WORLDBEYONDEDITOR_API FName GetName(int32 Layer);

	// Flat colour for the greybox material
	WORLDBEYONDEDITOR_API FLinearColor GetGreyboxColour(int32 Layer);

	// Colour on the painted world map
	WORLDBEYONDEDITOR_API FLinearColor GetMapColour(int32 Layer);
}

/**
 * A generated terrain: heights (metres) and 8-bit layer weights for every landscape vertex. Vertex (X, Y) sits at
 * East = -HalfExtent + X, North = HalfExtent - Y (row 0 is the north edge; the landscape's +Y is south).
 */
struct WORLDBEYONDEDITOR_API FBeyondHeightfield
{
	int32 Size = 0;
	float HalfExtent = 0.0f;

	// Metres, Size x Size
	TArray<float> Heights;

	// One Size x Size array per BeyondTerrainLayers entry; the layers add up to 255 at every vertex
	TArray<TArray<uint8>> Weights;

	// 255 on roads (the painted map draws them)
	TArray<uint8> RoadMask;

	// Region influence on a coarse grid (one array per layout region, CoarseSize x CoarseSize, every CoarseStep m)
	TArray<TArray<float>> RegionWeights;
	int32 CoarseSize = 0;
	float CoarseStep = 8.0f;

	// Water surface per lake id (m)
	TMap<FName, float> LakeLevels;

	bool bHasVolcano = false;
	float LavaLevel = 0.0f;

	int32 Index(int32 X, int32 Y) const { return Y * Size + X; }
	FVector2D VertexToMetres(int32 X, int32 Y) const { return FVector2D(-HalfExtent + X, HalfExtent - Y); }

	// Bilinear height at a point (metres east / north)
	float SampleHeight(const FVector2D& EastNorth) const;

	// Region weight (0..1) at a point
	float SampleRegionWeight(int32 RegionIndex, const FVector2D& EastNorth) const;

	// Landscape height data (32768 = 0 m) for a landscape Z scale
	TArray<uint16> ToLandscapeHeights(float HeightScaleZ) const;
};

/**
 * Builds the open world's terrain from a UBeyondWorldLayout (Plan 5B): regions blended into each other with their own
 * hills (rolling forest, low blight, ridged frost and fire), ridges with passes, peaks, the world's mountain border, a
 * volcano with a crater and a breach, lake basins, roads that keep a gentle slope, flat pads; then the ground layers by
 * region, slope, height, shore, road and pad. Deterministic for a seed.
 */
class WORLDBEYONDEDITOR_API FBeyondHeightfieldGenerator
{
public:
	static void Generate(const UBeyondWorldLayout& Layout, FBeyondHeightfield& Out);

	// Signed distance (m) from a point to a polygon's edge: positive inside
	static float SignedDistanceToPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon);

	// Distance (m) from a point to a polyline
	static float DistanceToPolyline(const FVector2D& Point, const TArray<FVector2D>& Points);
};
