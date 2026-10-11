// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondHeightfieldGenerator.h"
#include "WorldBeyondEditor.h"
#include "Async/ParallelFor.h"
#include "World/BeyondWorldLayout.h"

namespace BeyondHeightfieldLocal
{
	constexpr float RoadShoulder = 14.0f;
	constexpr float LakeShore = 45.0f;
	constexpr float RoadMaxGrade = 0.12f;
	constexpr float RoadStep = 4.0f;

	float SmoothStep01(float Edge0, float Edge1, float X)
	{
		if (FMath::IsNearlyEqual(Edge0, Edge1))
		{
			return X < Edge0 ? 0.0f : 1.0f;
		}
		const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	// Fractal noise in [-1, 1]
	float Fbm(const FVector2D& P, int32 Octaves, const FVector2D& Offset)
	{
		float Sum = 0.0f;
		float Amplitude = 0.5f;
		float Frequency = 1.0f;
		float Norm = 0.0f;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += Amplitude * FMath::PerlinNoise2D(P * Frequency + Offset * (Octave + 1));
			Norm += Amplitude;
			Amplitude *= 0.5f;
			Frequency *= 2.03f;
		}
		return Norm > 0.0f ? Sum / Norm : 0.0f;
	}

	// Sharp ridges in [0, 1]
	float Ridged(const FVector2D& P, int32 Octaves, const FVector2D& Offset)
	{
		float Sum = 0.0f;
		float Amplitude = 0.5f;
		float Frequency = 1.0f;
		float Norm = 0.0f;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			const float N = 1.0f - FMath::Abs(FMath::PerlinNoise2D(P * Frequency + Offset * (Octave + 1)));
			Sum += Amplitude * N * N;
			Norm += Amplitude;
			Amplitude *= 0.5f;
			Frequency *= 2.1f;
		}
		return Norm > 0.0f ? Sum / Norm : 0.0f;
	}

	float DistanceToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B, float& OutT)
	{
		const FVector2D AB = B - A;
		const double LengthSq = AB.SizeSquared();
		OutT = LengthSq > 0.0 ? FMath::Clamp(static_cast<float>(FVector2D::DotProduct(P - A, AB) / LengthSq), 0.0f, 1.0f) : 0.0f;
		return static_cast<float>(FVector2D::Distance(P, A + AB * OutT));
	}

	float HillsFor(EBeyondTerrainStyle Style, const FVector2D& P, float Scale, const FVector2D& Offset)
	{
		const FVector2D Q = P / FMath::Max(Scale, 1.0f);
		switch (Style)
		{
		case EBeyondTerrainStyle::Blight:
			return Fbm(Q, 4, Offset) * 0.8f - 0.25f * FMath::Max(0.0f, Fbm(Q * 3.0f, 2, Offset * 1.7f));
		case EBeyondTerrainStyle::Frost:
			return Ridged(Q, 5, Offset) * 1.4f - 0.55f + Fbm(Q * 0.5f, 3, Offset * 0.6f) * 0.4f;
		case EBeyondTerrainStyle::Molten:
			return Ridged(Q * 1.2f, 5, Offset * 1.3f) * 1.5f - 0.6f;
		default:
			return Fbm(Q, 5, Offset);
		}
	}

	// The layer mix a region's ground uses (before slope, height, roads...)
	void AddStyleWeights(EBeyondTerrainStyle Style, float Weight, float Noise, float* W)
	{
		using namespace BeyondTerrainLayers;
		const float N = Noise * 0.5f + 0.5f;
		switch (Style)
		{
		case EBeyondTerrainStyle::Blight:
			W[Corrupt] += Weight * (0.45f + 0.25f * N);
			W[Mud] += Weight * (0.35f - 0.2f * N);
			W[Moss] += Weight * 0.2f;
			break;
		case EBeyondTerrainStyle::Frost:
			W[Snow] += Weight * (0.7f + 0.2f * N);
			W[Moss] += Weight * (0.2f - 0.15f * N);
			W[Rock] += Weight * 0.1f;
			break;
		case EBeyondTerrainStyle::Molten:
			W[Ash] += Weight * (0.65f + 0.2f * N);
			W[Rock] += Weight * (0.35f - 0.2f * N);
			break;
		default:
			W[ForestGround] += Weight * (0.5f - 0.2f * N);
			W[Grass] += Weight * (0.3f + 0.3f * N);
			W[Moss] += Weight * 0.2f;
			break;
		}
	}
}

namespace BeyondTerrainLayers
{
	FName GetName(int32 Layer)
	{
		static const FName Names[Count] = { TEXT("ForestGround"), TEXT("Moss"), TEXT("Grass"), TEXT("Dirt"), TEXT("Path"), TEXT("Cobble"), TEXT("Mud"),
			TEXT("Rock"), TEXT("Snow"), TEXT("Ash"), TEXT("Corrupt") };
		return Layer >= 0 && Layer < Count ? Names[Layer] : NAME_None;
	}

	FLinearColor GetGreyboxColour(int32 Layer)
	{
		static const FLinearColor Colours[Count] = {
			FLinearColor(0.10f, 0.13f, 0.05f), FLinearColor(0.09f, 0.17f, 0.05f), FLinearColor(0.16f, 0.26f, 0.07f), FLinearColor(0.24f, 0.17f, 0.09f),
			FLinearColor(0.33f, 0.26f, 0.16f), FLinearColor(0.28f, 0.27f, 0.25f), FLinearColor(0.12f, 0.09f, 0.06f), FLinearColor(0.22f, 0.21f, 0.2f),
			FLinearColor(0.85f, 0.88f, 0.92f), FLinearColor(0.07f, 0.065f, 0.065f), FLinearColor(0.12f, 0.08f, 0.15f) };
		return Layer >= 0 && Layer < Count ? Colours[Layer] : FLinearColor::Gray;
	}

	FLinearColor GetMapColour(int32 Layer)
	{
		static const FLinearColor Colours[Count] = {
			FLinearColor(0.42f, 0.48f, 0.30f), FLinearColor(0.40f, 0.52f, 0.32f), FLinearColor(0.55f, 0.62f, 0.36f), FLinearColor(0.62f, 0.52f, 0.36f),
			FLinearColor(0.72f, 0.62f, 0.44f), FLinearColor(0.64f, 0.62f, 0.56f), FLinearColor(0.46f, 0.40f, 0.30f), FLinearColor(0.56f, 0.53f, 0.48f),
			FLinearColor(0.92f, 0.94f, 0.96f), FLinearColor(0.33f, 0.30f, 0.29f), FLinearColor(0.42f, 0.33f, 0.46f) };
		return Layer >= 0 && Layer < Count ? Colours[Layer] : FLinearColor::Gray;
	}
}

float FBeyondHeightfield::SampleHeight(const FVector2D& EastNorth) const
{
	if (Size <= 1 || Heights.Num() != Size * Size)
	{
		return 0.0f;
	}
	const float FX = FMath::Clamp(static_cast<float>(EastNorth.X + HalfExtent), 0.0f, static_cast<float>(Size - 1));
	const float FY = FMath::Clamp(static_cast<float>(HalfExtent - EastNorth.Y), 0.0f, static_cast<float>(Size - 1));
	const int32 X0 = FMath::Min(FMath::FloorToInt(FX), Size - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt(FY), Size - 2);
	const float TX = FX - X0;
	const float TY = FY - Y0;
	const float H00 = Heights[Index(X0, Y0)];
	const float H10 = Heights[Index(X0 + 1, Y0)];
	const float H01 = Heights[Index(X0, Y0 + 1)];
	const float H11 = Heights[Index(X0 + 1, Y0 + 1)];
	return FMath::Lerp(FMath::Lerp(H00, H10, TX), FMath::Lerp(H01, H11, TX), TY);
}

float FBeyondHeightfield::SampleRegionWeight(int32 RegionIndex, const FVector2D& EastNorth) const
{
	if (!RegionWeights.IsValidIndex(RegionIndex) || CoarseSize <= 1)
	{
		return 0.0f;
	}
	const TArray<float>& Field = RegionWeights[RegionIndex];
	const float FX = FMath::Clamp(static_cast<float>((EastNorth.X + HalfExtent) / CoarseStep), 0.0f, static_cast<float>(CoarseSize - 1));
	const float FY = FMath::Clamp(static_cast<float>((HalfExtent - EastNorth.Y) / CoarseStep), 0.0f, static_cast<float>(CoarseSize - 1));
	const int32 X0 = FMath::Min(FMath::FloorToInt(FX), CoarseSize - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt(FY), CoarseSize - 2);
	const float TX = FX - X0;
	const float TY = FY - Y0;
	const float W00 = Field[Y0 * CoarseSize + X0];
	const float W10 = Field[Y0 * CoarseSize + X0 + 1];
	const float W01 = Field[(Y0 + 1) * CoarseSize + X0];
	const float W11 = Field[(Y0 + 1) * CoarseSize + X0 + 1];
	return FMath::Lerp(FMath::Lerp(W00, W10, TX), FMath::Lerp(W01, W11, TX), TY);
}

TArray<uint16> FBeyondHeightfield::ToLandscapeHeights(float HeightScaleZ) const
{
	TArray<uint16> Result;
	Result.SetNumUninitialized(Heights.Num());
	const float PerMetre = 100.0f * 128.0f / FMath::Max(HeightScaleZ, 1.0f);
	for (int32 Index = 0; Index < Heights.Num(); ++Index)
	{
		Result[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(32768.0f + Heights[Index] * PerMetre), 0, 65535));
	}
	return Result;
}

float FBeyondHeightfieldGenerator::SignedDistanceToPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return -TNumericLimits<float>::Max();
	}
	bool bInside = false;
	float Best = TNumericLimits<float>::Max();
	for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[Previous];
		if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
		{
			bInside = !bInside;
		}
		float T;
		Best = FMath::Min(Best, BeyondHeightfieldLocal::DistanceToSegment(Point, A, B, T));
	}
	return bInside ? Best : -Best;
}

float FBeyondHeightfieldGenerator::DistanceToPolyline(const FVector2D& Point, const TArray<FVector2D>& Points)
{
	float Best = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		float T;
		Best = FMath::Min(Best, BeyondHeightfieldLocal::DistanceToSegment(Point, Points[Index], Points[Index + 1], T));
	}
	if (Points.Num() == 1)
	{
		Best = static_cast<float>(FVector2D::Distance(Point, Points[0]));
	}
	return Best;
}

void FBeyondHeightfieldGenerator::Generate(const UBeyondWorldLayout& Layout, FBeyondHeightfield& Out)
{
	using namespace BeyondHeightfieldLocal;
	const double StartTime = FPlatformTime::Seconds();
	const int32 Size = Layout.GetSize();
	const float Half = Layout.GetHalfExtent();
	const int32 Count = Size * Size;
	Out = FBeyondHeightfield();
	Out.Size = Size;
	Out.HalfExtent = Half;
	Out.Heights.SetNumZeroed(Count);

	FRandomStream Random(Layout.Seed);
	auto RandomOffset = [&Random]() { return FVector2D(Random.FRandRange(-500.0f, 500.0f), Random.FRandRange(-500.0f, 500.0f)); };
	const FVector2D DetailOffset = RandomOffset();
	TArray<FVector2D> RegionOffsets;
	for (int32 Region = 0; Region < Layout.Regions.Num(); ++Region)
	{
		RegionOffsets.Add(RandomOffset());
	}
	const FVector2D BorderOffset = RandomOffset();
	const FVector2D PeakOffset = RandomOffset();
	const FVector2D CraterOffset = RandomOffset();
	const FVector2D WarpOffsetX = RandomOffset();
	const FVector2D WarpOffsetY = RandomOffset();
	// Borders and ridges wander off their straight lines (less near a pass, so the gap stays where the road is)
	auto Warp = [&Layout, WarpOffsetX, WarpOffsetY](const FVector2D& P)
	{
		const FVector2D Q = P / FMath::Max(Layout.BorderWarpScale, 50.0f);
		float Amount = Layout.BorderWarp;
		for (const FBeyondLayoutRidge& Ridge : Layout.Ridges)
		{
			for (const FVector2D& Pass : Ridge.Passes)
			{
				Amount *= SmoothStep01(Ridge.PassWidth * 1.2f, Ridge.PassWidth * 2.6f, static_cast<float>(FVector2D::Distance(P, Pass)));
			}
		}
		// Fractal noise rarely leaves +-0.4: scaled so Border Warp is about the real wander
		return FVector2D(Fbm(Q, 3, WarpOffsetX), Fbm(Q, 3, WarpOffsetY)) * Amount * 2.5f;
	};

	//~ Region influence on a coarse grid
	const int32 NumRegions = Layout.Regions.Num();
	Out.CoarseStep = 8.0f;
	Out.CoarseSize = FMath::CeilToInt((Size - 1) / Out.CoarseStep) + 1;
	const int32 CoarseCount = Out.CoarseSize * Out.CoarseSize;
	Out.RegionWeights.SetNum(NumRegions);
	for (TArray<float>& Field : Out.RegionWeights)
	{
		Field.SetNumZeroed(CoarseCount);
	}
	const float Blend = FMath::Max(Layout.RegionBlend, 1.0f);
	ParallelFor(Out.CoarseSize, [&](int32 CY)
	{
		TArray<float> Distances;
		Distances.SetNum(NumRegions);
		for (int32 CX = 0; CX < Out.CoarseSize; ++CX)
		{
			const FVector2D P(-Half + CX * Out.CoarseStep, Half - CY * Out.CoarseStep);
			const FVector2D Wandered = P + Warp(P);
			float Sum = 0.0f;
			int32 Nearest = INDEX_NONE;
			float NearestDistance = -TNumericLimits<float>::Max();
			for (int32 Region = 0; Region < NumRegions; ++Region)
			{
				Distances[Region] = SignedDistanceToPolygon(Wandered, Layout.Regions[Region].Polygon);
				if (Distances[Region] > NearestDistance)
				{
					NearestDistance = Distances[Region];
					Nearest = Region;
				}
				const float W = SmoothStep01(-Blend * 0.5f, Blend * 0.5f, Distances[Region]);
				Out.RegionWeights[Region][CY * Out.CoarseSize + CX] = W;
				Sum += W;
			}
			for (int32 Region = 0; Region < NumRegions; ++Region)
			{
				float& W = Out.RegionWeights[Region][CY * Out.CoarseSize + CX];
				W = Sum > 0.001f ? W / Sum : (Region == Nearest ? 1.0f : 0.0f);
			}
		}
	});

	//~ 1. The ground: regions, ridges, peaks, the border, the volcano
	const FBeyondLayoutVolcano& Volcano = Layout.Volcano;
	ParallelFor(Size, [&](int32 Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const FVector2D P(-Half + X, Half - Y);
			float H = 0.0f;
			float WeightSum = 0.0f;
			for (int32 Region = 0; Region < NumRegions; ++Region)
			{
				const float W = Out.SampleRegionWeight(Region, P);
				if (W <= 0.001f)
				{
					continue;
				}
				const FBeyondLayoutRegion& Def = Layout.Regions[Region];
				H += W * (Def.BaseHeight + Def.HillHeight * HillsFor(Def.Style, P, Def.HillScale, RegionOffsets[Region]));
				WeightSum += W;
			}
			H = WeightSum > 0.0f ? H / WeightSum : 20.0f;
			H += 1.2f * Fbm(P / 45.0f, 3, DetailOffset);

			const FVector2D Wandered = Layout.Ridges.IsEmpty() ? P : P + Warp(P);
			for (const FBeyondLayoutRidge& Ridge : Layout.Ridges)
			{
				const float D = DistanceToPolyline(Wandered, Ridge.Points);
				const float HalfWidth = Ridge.Width * 0.5f;
				if (D >= HalfWidth)
				{
					continue;
				}
				float Gap = 0.0f;
				for (const FVector2D& Pass : Ridge.Passes)
				{
					Gap = FMath::Max(Gap, SmoothStep01(Ridge.PassWidth, Ridge.PassWidth * 0.45f, static_cast<float>(FVector2D::Distance(P, Pass))));
				}
				const float Profile = SmoothStep01(HalfWidth, 0.0f, D);
				H += Ridge.Height * Profile * (1.0f - Gap) * (0.85f + 0.3f * Fbm(P / 90.0f, 2, BorderOffset));
			}

			for (const FBeyondLayoutPeak& Peak : Layout.Peaks)
			{
				const float D = static_cast<float>(FVector2D::Distance(P, Peak.Centre));
				if (D < Peak.Radius)
				{
					const float T = 1.0f - D / Peak.Radius;
					H += Peak.Height * FMath::Pow(SmoothStep01(0.0f, 1.0f, T), Peak.Sharpness) * (0.8f + 0.35f * Ridged(P / 120.0f, 3, PeakOffset));
				}
			}

			const float Edge = static_cast<float>(FMath::Min(FMath::Min(P.X + Half, Half - P.X), FMath::Min(P.Y + Half, Half - P.Y)));
			if (Edge < Layout.BorderWidth)
			{
				const float T = SmoothStep01(Layout.BorderWidth, 0.0f, Edge);
				H += Layout.BorderHeight * FMath::Pow(T, 1.4f) * (0.75f + 0.4f * Ridged(P / 160.0f, 3, BorderOffset));
			}

			if (Volcano.bEnabled)
			{
				const FVector2D ToPoint = P - Volcano.Centre;
				const float D = static_cast<float>(ToPoint.Size());
				if (D < Volcano.Radius)
				{
					// Bearing: 0 north, 90 east
					const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(ToPoint.X), static_cast<float>(ToPoint.Y)));
					const float AngleOff = FMath::Abs(FMath::FindDeltaAngleDegrees(Bearing, Volcano.BreachBearing));
					const float Breach = SmoothStep01(Volcano.BreachHalfAngle * 1.6f, Volcano.BreachHalfAngle * 0.7f, AngleOff);
					const float Rim = FMath::Lerp(Volcano.RimHeight, Volcano.FloorHeight + 3.0f, Breach);
					if (D >= Volcano.RimRadius)
					{
						const float T = (Volcano.Radius - D) / FMath::Max(Volcano.Radius - Volcano.RimRadius, 1.0f);
						H = FMath::Lerp(H, Rim + 8.0f * Fbm(P / 60.0f, 3, CraterOffset), FMath::Pow(SmoothStep01(0.0f, 1.0f, T), 1.3f));
					}
					else
					{
						const float S = D / Volcano.RimRadius;
						const float Floor = Volcano.FloorHeight + 6.0f * Fbm(P / 35.0f, 3, CraterOffset);
						H = FMath::Lerp(Floor, Rim, SmoothStep01(0.55f, 1.0f, S));
					}
				}
			}
			Out.Heights[Out.Index(X, Y)] = H;
		}
	});
	Out.bHasVolcano = Volcano.bEnabled;
	Out.LavaLevel = Volcano.LavaLevel;

	//~ 2. Lakes: a bowl below the ground at their centre, a muddy shore
	TArray<float> Shore;
	Shore.SetNumZeroed(Count);
	for (const FBeyondLayoutLake& Lake : Layout.Lakes)
	{
		const float Level = Out.SampleHeight(Lake.Centre) - 1.5f;
		Out.LakeLevels.Add(Lake.Id, Level);
		const float Outer = Lake.Radius + LakeShore;
		const int32 MinX = FMath::Clamp(FMath::FloorToInt(Lake.Centre.X - Outer + Half), 0, Size - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt(Lake.Centre.X + Outer + Half), 0, Size - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt(Half - Lake.Centre.Y - Outer), 0, Size - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt(Half - Lake.Centre.Y + Outer), 0, Size - 1);
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector2D P = Out.VertexToMetres(X, Y);
				const float D = static_cast<float>(FVector2D::Distance(P, Lake.Centre));
				float& H = Out.Heights[Out.Index(X, Y)];
				if (D < Lake.Radius)
				{
					const float R = D / Lake.Radius;
					H = FMath::Min(H, Level - Lake.Depth * (1.0f - R * R) - 0.6f);
					Shore[Out.Index(X, Y)] = 1.0f;
				}
				else if (D < Outer)
				{
					const float T = SmoothStep01(Lake.Radius, Outer, D);
					H = FMath::Lerp(FMath::Min(H, Level + 0.4f), H, T);
					Shore[Out.Index(X, Y)] = FMath::Max(Shore[Out.Index(X, Y)], 1.0f - T);
				}
			}
		}
	}

	//~ 3. Roads: a smoothed, gently sloped profile along each, the ground pressed to it
	TArray<float> RoadDistance;
	TArray<float> RoadTarget;
	TArray<uint8> RoadIndex;
	RoadDistance.Init(TNumericLimits<float>::Max(), Count);
	RoadTarget.SetNumZeroed(Count);
	RoadIndex.Init(255, Count);
	for (int32 RoadNumber = 0; RoadNumber < Layout.Roads.Num() && RoadNumber < 255; ++RoadNumber)
	{
		const FBeyondLayoutRoad& Road = Layout.Roads[RoadNumber];
		TArray<FVector2D> Points;
		for (int32 Index = 0; Index + 1 < Road.Points.Num(); ++Index)
		{
			const FVector2D A = Road.Points[Index];
			const FVector2D B = Road.Points[Index + 1];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / RoadStep));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Points.Add(FMath::Lerp(A, B, static_cast<double>(Step) / Steps));
			}
		}
		if (!Road.Points.IsEmpty())
		{
			Points.Add(Road.Points.Last());
		}
		if (Points.Num() < 2)
		{
			continue;
		}
		TArray<float> Profile;
		for (const FVector2D& Point : Points)
		{
			Profile.Add(Out.SampleHeight(Point));
		}
		// Smooth twice (about 50 m), then keep every step within the grade limit both ways
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			TArray<float> Smoothed = Profile;
			for (int32 Index = 0; Index < Profile.Num(); ++Index)
			{
				float Sum = 0.0f;
				int32 Samples = 0;
				for (int32 K = FMath::Max(0, Index - 6); K <= FMath::Min(Profile.Num() - 1, Index + 6); ++K)
				{
					Sum += Profile[K];
					++Samples;
				}
				Smoothed[Index] = Sum / Samples;
			}
			Profile = MoveTemp(Smoothed);
		}
		const float MaxStep = RoadMaxGrade * RoadStep;
		for (int32 Index = 1; Index < Profile.Num(); ++Index)
		{
			Profile[Index] = FMath::Clamp(Profile[Index], Profile[Index - 1] - MaxStep, Profile[Index - 1] + MaxStep);
		}
		for (int32 Index = Profile.Num() - 2; Index >= 0; --Index)
		{
			Profile[Index] = FMath::Clamp(Profile[Index], Profile[Index + 1] - MaxStep, Profile[Index + 1] + MaxStep);
		}

		const float Reach = Road.Width * 0.5f + RoadShoulder;
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			const FVector2D A = Points[Index];
			const FVector2D B = Points[Index + 1];
			const int32 MinX = FMath::Clamp(FMath::FloorToInt(FMath::Min(A.X, B.X) - Reach + Half), 0, Size - 1);
			const int32 MaxX = FMath::Clamp(FMath::CeilToInt(FMath::Max(A.X, B.X) + Reach + Half), 0, Size - 1);
			const int32 MinY = FMath::Clamp(FMath::FloorToInt(Half - FMath::Max(A.Y, B.Y) - Reach), 0, Size - 1);
			const int32 MaxY = FMath::Clamp(FMath::CeilToInt(Half - FMath::Min(A.Y, B.Y) + Reach), 0, Size - 1);
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					float T;
					const float D = DistanceToSegment(Out.VertexToMetres(X, Y), A, B, T);
					const int32 Vertex = Out.Index(X, Y);
					if (D < Reach && D < RoadDistance[Vertex])
					{
						RoadDistance[Vertex] = D;
						RoadTarget[Vertex] = FMath::Lerp(Profile[Index], Profile[Index + 1], T);
						RoadIndex[Vertex] = static_cast<uint8>(RoadNumber);
					}
				}
			}
		}
	}
	Out.RoadMask.SetNumZeroed(Count);
	TArray<float> RoadSurface;
	RoadSurface.SetNumZeroed(Count);
	ParallelFor(Count, [&](int32 Vertex)
	{
		if (RoadIndex[Vertex] == 255)
		{
			return;
		}
		const FBeyondLayoutRoad& Road = Layout.Roads[RoadIndex[Vertex]];
		const float HalfWidth = Road.Width * 0.5f;
		const float D = RoadDistance[Vertex];
		const float Press = 1.0f - SmoothStep01(HalfWidth, HalfWidth + RoadShoulder, D);
		Out.Heights[Vertex] = FMath::Lerp(Out.Heights[Vertex], RoadTarget[Vertex] - 0.15f, Press);
		const float Surface = 1.0f - SmoothStep01(HalfWidth * 0.7f, HalfWidth, D);
		RoadSurface[Vertex] = Surface;
		Out.RoadMask[Vertex] = static_cast<uint8>(FMath::RoundToInt(Surface * 255.0f));
	});
	RoadDistance.Empty();
	RoadTarget.Empty();

	//~ 4. Pads: flat at the ground's average under them (later pads win)
	TArray<float> PadMask;
	TArray<uint8> PadSurface;
	PadMask.SetNumZeroed(Count);
	PadSurface.Init(static_cast<uint8>(EBeyondPadSurface::Ground), Count);
	for (const FBeyondLayoutPad& Pad : Layout.Pads)
	{
		float Sum = 0.0f;
		int32 Samples = 0;
		for (float DY = -Pad.Radius; DY <= Pad.Radius; DY += 4.0f)
		{
			for (float DX = -Pad.Radius; DX <= Pad.Radius; DX += 4.0f)
			{
				if (DX * DX + DY * DY <= Pad.Radius * Pad.Radius)
				{
					Sum += Out.SampleHeight(Pad.Centre + FVector2D(DX, DY));
					++Samples;
				}
			}
		}
		const float Target = (Samples > 0 ? Sum / Samples : Out.SampleHeight(Pad.Centre)) + Pad.HeightOffset;
		const float Outer = Pad.Radius + Pad.Falloff;
		const int32 MinX = FMath::Clamp(FMath::FloorToInt(Pad.Centre.X - Outer + Half), 0, Size - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt(Pad.Centre.X + Outer + Half), 0, Size - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt(Half - Pad.Centre.Y - Outer), 0, Size - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt(Half - Pad.Centre.Y + Outer), 0, Size - 1);
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const float D = static_cast<float>(FVector2D::Distance(Out.VertexToMetres(X, Y), Pad.Centre));
				if (D >= Outer)
				{
					continue;
				}
				const int32 Vertex = Out.Index(X, Y);
				const float Flat = 1.0f - SmoothStep01(Pad.Radius, Outer, D);
				Out.Heights[Vertex] = FMath::Lerp(Out.Heights[Vertex], Target, Flat);
				const float Surface = 1.0f - SmoothStep01(Pad.Radius * 0.75f, Pad.Radius * 1.05f, D);
				if (Surface > PadMask[Vertex])
				{
					PadMask[Vertex] = Surface;
					PadSurface[Vertex] = static_cast<uint8>(Pad.Surface);
				}
			}
		}
	}

	//~ 5. Ground layers
	using namespace BeyondTerrainLayers;
	Out.Weights.SetNum(BeyondTerrainLayers::Count);
	for (TArray<uint8>& Layer : Out.Weights)
	{
		Layer.SetNumZeroed(Count);
	}
	const FVector2D LayerNoiseOffset = RandomOffset();
	ParallelFor(Size, [&](int32 Y)
	{
		float W[BeyondTerrainLayers::Count];
		for (int32 X = 0; X < Size; ++X)
		{
			const int32 Vertex = Out.Index(X, Y);
			const FVector2D P = Out.VertexToMetres(X, Y);
			const float H = Out.Heights[Vertex];
			FMemory::Memzero(W, sizeof(W));

			const float Noise = Fbm(P / 28.0f, 3, LayerNoiseOffset);
			bool bMolten = false;
			for (int32 Region = 0; Region < NumRegions; ++Region)
			{
				const float RegionWeight = Out.SampleRegionWeight(Region, P);
				if (RegionWeight > 0.001f)
				{
					AddStyleWeights(Layout.Regions[Region].Style, RegionWeight, Noise, W);
					bMolten |= Layout.Regions[Region].Style == EBeyondTerrainStyle::Molten && RegionWeight > 0.5f;
				}
			}
			auto Push = [&W](int32 Layer, float Amount)
			{
				Amount = FMath::Clamp(Amount, 0.0f, 1.0f);
				if (Amount <= 0.0f)
				{
					return;
				}
				for (int32 Other = 0; Other < BeyondTerrainLayers::Count; ++Other)
				{
					W[Other] *= 1.0f - Amount;
				}
				W[Layer] += Amount;
			};

			// Slope (degrees) from the neighbours
			const float HX0 = Out.Heights[Out.Index(FMath::Max(X - 1, 0), Y)];
			const float HX1 = Out.Heights[Out.Index(FMath::Min(X + 1, Size - 1), Y)];
			const float HY0 = Out.Heights[Out.Index(X, FMath::Max(Y - 1, 0))];
			const float HY1 = Out.Heights[Out.Index(X, FMath::Min(Y + 1, Size - 1))];
			const float Gradient = FMath::Sqrt(FMath::Square((HX1 - HX0) * 0.5f) + FMath::Square((HY1 - HY0) * 0.5f));
			const float Slope = FMath::RadiansToDegrees(FMath::Atan(Gradient));

			if (!bMolten)
			{
				Push(Snow, SmoothStep01(205.0f, 245.0f, H + Noise * 12.0f));
			}
			if (Out.bHasVolcano && H < Out.LavaLevel + 2.0f && FVector2D::Distance(P, Layout.Volcano.Centre) < Layout.Volcano.RimRadius)
			{
				Push(Ash, 1.0f);
			}
			Push(Mud, Shore[Vertex] * 0.85f);
			Push(Rock, SmoothStep01(28.0f, 40.0f, Slope + Noise * 4.0f));
			if (RoadIndex[Vertex] != 255)
			{
				Push(Layout.Roads[RoadIndex[Vertex]].bCobble ? Cobble : Path, RoadSurface[Vertex]);
			}
			switch (static_cast<EBeyondPadSurface>(PadSurface[Vertex]))
			{
			case EBeyondPadSurface::Dirt: Push(Dirt, PadMask[Vertex] * 0.9f); break;
			case EBeyondPadSurface::Cobble: Push(Cobble, PadMask[Vertex]); break;
			case EBeyondPadSurface::Rock: Push(Rock, PadMask[Vertex] * 0.85f); break;
			default: break;
			}

			// To bytes adding up to exactly 255
			float Sum = 0.0f;
			for (const float Value : W)
			{
				Sum += FMath::Max(Value, 0.0f);
			}
			if (Sum <= 0.0f)
			{
				W[ForestGround] = Sum = 1.0f;
			}
			int32 Total = 0;
			int32 Largest = 0;
			for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
			{
				const int32 Byte = FMath::FloorToInt(FMath::Max(W[Layer], 0.0f) / Sum * 255.0f);
				Out.Weights[Layer][Vertex] = static_cast<uint8>(Byte);
				Total += Byte;
				if (W[Layer] > W[Largest])
				{
					Largest = Layer;
				}
			}
			Out.Weights[Largest][Vertex] = static_cast<uint8>(Out.Weights[Largest][Vertex] + (255 - Total));
		}
	});

	UE_LOG(LogBeyondEditor, Display, TEXT("World builder: generated a %d x %d heightfield in %.1f s"), Size, Size, FPlatformTime::Seconds() - StartTime);
}
