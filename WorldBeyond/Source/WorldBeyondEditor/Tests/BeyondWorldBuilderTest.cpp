// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "LandscapeGrassType.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "World/BeyondFloraLibrary.h"
#include "World/BeyondGroundLibrary.h"
#include "World/BeyondHeightfieldGenerator.h"
#include "World/BeyondWorldLayout.h"

/**
 * Plan 5B's terrain generator on a small layout (no map, no PIE): the same seed gives the same terrain, pads are flat,
 * roads keep to the grade limit, lake floors sit under the water, layer weights add up to 255, ridges rise and their
 * passes cut through, the volcano's crater is lower than its rim. Plan 5D's ground: the region mask reads frost in the
 * frost region and nothing in the forest, the ground library knows the 11 layers, and once pass 18 has built
 * M_Beyond_Landscape its three height blends list every layer and it grows its six grass types. Plan 5E's flora scatter:
 * the same seed scatters the same; trees stay in their region, off the road and out of the village, apart by their
 * footprints; reeds only on the lake's muddy shore; cliffs only on steep rock.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype.WorldBuilder;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondWorldBuilderTest
{
	UBeyondWorldLayout* MakeLayout(int32 Seed)
	{
		UBeyondWorldLayout* Layout = NewObject<UBeyondWorldLayout>();
		Layout->Seed = Seed;
		// 8 components x 2 sections x 31 quads = 497 m across
		Layout->ComponentsPerSide = 8;
		Layout->SectionsPerComponent = 2;
		Layout->QuadsPerSection = 31;
		Layout->BorderWidth = 40.0f;
		Layout->BorderHeight = 60.0f;
		Layout->RegionBlend = 40.0f;
		// Straight borders, so the ridge checks know where it is
		Layout->BorderWarp = 0.0f;

		FBeyondLayoutRegion& West = Layout->Regions.AddDefaulted_GetRef();
		West.RegionId = TEXT("west");
		West.Style = EBeyondTerrainStyle::Forest;
		West.Polygon = { FVector2D(-250, -250), FVector2D(0, -250), FVector2D(0, 250), FVector2D(-250, 250) };
		West.BaseHeight = 20.0f;
		West.HillHeight = 12.0f;
		West.HillScale = 80.0f;
		FBeyondLayoutRegion& East = Layout->Regions.AddDefaulted_GetRef();
		East.RegionId = TEXT("east");
		East.Style = EBeyondTerrainStyle::Frost;
		East.Polygon = { FVector2D(0, -250), FVector2D(250, -250), FVector2D(250, 250), FVector2D(0, 250) };
		East.BaseHeight = 40.0f;
		East.HillHeight = 20.0f;
		East.HillScale = 90.0f;

		FBeyondLayoutRidge& Ridge = Layout->Ridges.AddDefaulted_GetRef();
		Ridge.Points = { FVector2D(0, -200), FVector2D(0, 200) };
		Ridge.Height = 50.0f;
		Ridge.Width = 50.0f;
		Ridge.Passes = { FVector2D(0, -100) };
		Ridge.PassWidth = 30.0f;

		Layout->Volcano.bEnabled = true;
		Layout->Volcano.Centre = FVector2D(140, 140);
		Layout->Volcano.Radius = 80.0f;
		Layout->Volcano.RimRadius = 35.0f;
		Layout->Volcano.RimHeight = 90.0f;
		Layout->Volcano.FloorHeight = 60.0f;
		Layout->Volcano.LavaLevel = 58.0f;

		FBeyondLayoutLake& Lake = Layout->Lakes.AddDefaulted_GetRef();
		Lake.Id = TEXT("lake");
		Lake.Centre = FVector2D(-120, 100);
		Lake.Radius = 30.0f;
		Lake.Depth = 4.0f;

		FBeyondLayoutRoad& Road = Layout->Roads.AddDefaulted_GetRef();
		Road.Id = TEXT("road");
		Road.Points = { FVector2D(-180, -150), FVector2D(-60, -100), FVector2D(0, -100), FVector2D(150, -60) };
		Road.Width = 6.0f;

		FBeyondLayoutPad& Pad = Layout->Pads.AddDefaulted_GetRef();
		Pad.Id = TEXT("village");
		Pad.Centre = FVector2D(-120, -40);
		Pad.Radius = 20.0f;
		Pad.Falloff = 15.0f;
		return Layout;
	}

	// Plan 5E's scatter on the small layout (engine cubes for meshes, the generated terrain for heights)
	void CheckFlora(FAutomationTestBase& T, UBeyondWorldLayout* Layout, const FBeyondHeightfield& Field)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (!T.TestNotNull(TEXT("The engine cube loads (the flora's test mesh)"), Cube))
		{
			return;
		}
		auto Make = [Cube](const TCHAR* Name, EBeyondFloraKind Kind, float Density)
		{
			FBeyondFloraPlanting Planting;
			Planting.Name = Name;
			Planting.Kind = Kind;
			FBeyondFloraMesh& Mesh = Planting.Meshes.AddDefaulted_GetRef();
			Mesh.Mesh = Cube;
			Planting.Density = Density;
			Planting.AvoidLayers = { TEXT("Path"), TEXT("Cobble"), TEXT("Dirt") };
			return Planting;
		};
		FBeyondFloraPlanting Cliffs = Make(TEXT("cliffs"), EBeyondFloraKind::Cliff, 6.0f);
		Cliffs.Regions = FLinearColor(1.0f, 1.0f, 0.0f, 0.0f);
		Cliffs.Layers = { TEXT("Rock") };
		Cliffs.MinLayerWeight = 0.35f;
		Cliffs.MinSlope = 26.0f;
		Cliffs.MaxSlope = 80.0f;
		Cliffs.Scale = FVector2D(3.0f, 5.0f);
		FBeyondFloraPlanting Trees = Make(TEXT("trees"), EBeyondFloraKind::Tree, 40.0f);
		Trees.Regions = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
		Trees.GroveMax = 0.6f;
		Trees.Size = FVector2D(8.0f, 12.0f);
		Trees.Footprint = 1.5f;
		Trees.RoadClearance = 4.0f;
		FBeyondFloraPlanting Reeds = Make(TEXT("reeds"), EBeyondFloraKind::Shrub, 600.0f);
		Reeds.Regions = FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
		Reeds.Layers = { TEXT("Mud") };
		Reeds.MinLayerWeight = 0.3f;
		Reeds.bShore = true;
		Reeds.Footprint = 0.4f;
		Reeds.Scale = FVector2D(0.02f, 0.03f);
		const TArray<FBeyondFloraPlanting> Plantings = { Cliffs, Trees, Reeds };
		FBeyondFloraExclusion Village;
		Village.Id = TEXT("village");
		Village.Centre = Layout->Pads[0].Centre;
		Village.Radius = 30.0f;
		const TArray<FBeyondFloraExclusion> Exclusions = { Village };

		const TArray<FBeyondFloraBatch> First = UBeyondFloraLibrary::ComputeFlora(nullptr, Layout, Plantings, Exclusions, 5, {}, 60.0f);
		const TArray<FBeyondFloraBatch> Again = UBeyondFloraLibrary::ComputeFlora(nullptr, Layout, Plantings, Exclusions, 5, {}, 60.0f);
		bool bSame = First.Num() == Again.Num();
		for (int32 Index = 0; bSame && Index < First.Num(); ++Index)
		{
			bSame = First[Index].Transforms.Num() == Again[Index].Transforms.Num();
			for (int32 Item = 0; bSame && Item < First[Index].Transforms.Num(); ++Item)
			{
				bSame = First[Index].Transforms[Item].Equals(Again[Index].Transforms[Item], 0.01);
			}
		}
		T.TestTrue(TEXT("Flora: the same seed scatters the same"), bSame);

		const float* Level = Field.LakeLevels.Find(TEXT("lake"));
		const FBeyondLayoutRoad& Road = Layout->Roads[0];
		const FBeyondLayoutLake& Lake = Layout->Lakes[0];
		TMap<FName, int32> Counts;
		TArray<FVector2D> TreeSpots;
		int32 OnRoad = 0, InVillage = 0, TreesInFrost = 0, ReedsAway = 0, ReedsOffMud = 0, FlatCliffs = 0;
		for (const FBeyondFloraBatch& Batch : First)
		{
			for (const FTransform& Transform : Batch.Transforms)
			{
				const FVector2D P(Transform.GetLocation().X / 100.0, -Transform.GetLocation().Y / 100.0);
				const int32 Vertex = Field.Index(FMath::RoundToInt(P.X + Field.HalfExtent), FMath::RoundToInt(Field.HalfExtent - P.Y));
				++Counts.FindOrAdd(Batch.Planting);
				InVillage += FVector2D::Distance(P, Village.Centre) < Village.Radius ? 1 : 0;
				if (Batch.Planting == TEXT("trees"))
				{
					TreeSpots.Add(P);
					OnRoad += FBeyondHeightfieldGenerator::DistanceToPolyline(P, Road.Points) < Road.Width * 0.5f + 4.0f ? 1 : 0;
					// The frost region starts at x = 0, blended over 40 m
					TreesInFrost += P.X > 45.0 ? 1 : 0;
				}
				else if (Batch.Planting == TEXT("reeds"))
				{
					ReedsAway += FVector2D::Distance(P, Lake.Centre) > Lake.Radius + 60.0f || (Level && Field.SampleHeight(P) < *Level - 0.5f) ? 1 : 0;
					ReedsOffMud += Field.Weights[BeyondTerrainLayers::Mud][Vertex] < 70 ? 1 : 0;
				}
				else if (Batch.Planting == TEXT("cliffs"))
				{
					const float Rise = FVector2D(Field.SampleHeight(P + FVector2D(1.0, 0.0)) - Field.SampleHeight(P - FVector2D(1.0, 0.0)),
						Field.SampleHeight(P + FVector2D(0.0, 1.0)) - Field.SampleHeight(P - FVector2D(0.0, 1.0))).Size() * 0.5f;
					FlatCliffs += FMath::RadiansToDegrees(FMath::Atan(Rise)) < 25.5f ? 1 : 0;
				}
			}
		}
		int32 Overlaps = 0;
		for (int32 Index = 0; Index < TreeSpots.Num(); ++Index)
		{
			for (int32 Other = Index + 1; Other < TreeSpots.Num(); ++Other)
			{
				Overlaps += FVector2D::Distance(TreeSpots[Index], TreeSpots[Other]) < 2.99f ? 1 : 0;
			}
		}
		T.TestTrue(*FString::Printf(TEXT("Flora: trees in the forest (%d), reeds by the lake (%d), cliffs on the ridge (%d)"),
			Counts.FindRef(TEXT("trees")), Counts.FindRef(TEXT("reeds")), Counts.FindRef(TEXT("cliffs"))),
			Counts.FindRef(TEXT("trees")) > 50 && Counts.FindRef(TEXT("reeds")) > 5 && Counts.FindRef(TEXT("cliffs")) > 0);
		T.TestEqual(TEXT("Flora: no tree on or beside the road"), OnRoad, 0);
		T.TestEqual(TEXT("Flora: nothing in the village's clearing"), InVillage, 0);
		T.TestEqual(TEXT("Flora: no forest tree in the frost region"), TreesInFrost, 0);
		T.TestEqual(TEXT("Flora: no two trunks closer than their footprints"), Overlaps, 0);
		T.TestEqual(TEXT("Flora: reeds only on the lake's muddy shore"), ReedsAway + ReedsOffMud, 0);
		T.TestEqual(TEXT("Flora: cliffs only on steep ground"), FlatCliffs, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondWorldBuilderTest, "WorldsBeyond.Prototype.WorldBuilder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondWorldBuilderTest::RunTest(const FString& Parameters)
{
	using namespace BeyondWorldBuilderTest;
	UBeyondWorldLayout* Layout = MakeLayout(42);
	FBeyondHeightfield A;
	FBeyondHeightfield B;
	FBeyondHeightfieldGenerator::Generate(*Layout, A);
	FBeyondHeightfieldGenerator::Generate(*Layout, B);
	TestEqual(TEXT("497 x 497 vertices"), A.Size, 497);
	TestTrue(TEXT("The same seed gives the same terrain"), A.Heights == B.Heights && A.Weights == B.Weights);

	FBeyondHeightfield Other;
	FBeyondHeightfieldGenerator::Generate(*MakeLayout(7), Other);
	TestFalse(TEXT("Another seed gives another terrain"), Other.Heights == A.Heights);

	// The pad is flat
	const FBeyondLayoutPad& Pad = Layout->Pads[0];
	float Low = TNumericLimits<float>::Max();
	float High = -TNumericLimits<float>::Max();
	for (float DY = -Pad.Radius * 0.9f; DY <= Pad.Radius * 0.9f; DY += 2.0f)
	{
		for (float DX = -Pad.Radius * 0.9f; DX <= Pad.Radius * 0.9f; DX += 2.0f)
		{
			if (DX * DX + DY * DY <= FMath::Square(Pad.Radius * 0.9f))
			{
				const float H = A.SampleHeight(Pad.Centre + FVector2D(DX, DY));
				Low = FMath::Min(Low, H);
				High = FMath::Max(High, H);
			}
		}
	}
	TestTrue(*FString::Printf(TEXT("The village pad is flat (within 5 cm: %.3f m)"), High - Low), High - Low <= 0.05f);

	// The road keeps to the grade (12 %, measured over 8 m steps along it; a little slack for blending)
	float SteepestGrade = 0.0f;
	const TArray<FVector2D>& RoadPoints = Layout->Roads[0].Points;
	for (int32 Index = 0; Index + 1 < RoadPoints.Num(); ++Index)
	{
		const FVector2D From = RoadPoints[Index];
		const FVector2D To = RoadPoints[Index + 1];
		const int32 Steps = FMath::Max(1, FMath::FloorToInt(FVector2D::Distance(From, To) / 8.0f));
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const FVector2D P0 = FMath::Lerp(From, To, static_cast<double>(Step) / Steps);
			const FVector2D P1 = FMath::Lerp(From, To, static_cast<double>(Step + 1) / Steps);
			const float Grade = FMath::Abs(A.SampleHeight(P1) - A.SampleHeight(P0)) / static_cast<float>(FVector2D::Distance(P0, P1));
			SteepestGrade = FMath::Max(SteepestGrade, Grade);
		}
	}
	TestTrue(*FString::Printf(TEXT("The road keeps to a 12 %% grade (steepest %.1f %%)"), SteepestGrade * 100.0f), SteepestGrade <= 0.14f);

	// The lake
	const float* Level = A.LakeLevels.Find(TEXT("lake"));
	TestTrue(TEXT("The lake has a water level"), Level != nullptr);
	if (Level)
	{
		TestTrue(TEXT("The lake's middle is under the water"), A.SampleHeight(Layout->Lakes[0].Centre) < *Level - 2.0f);
	}

	// Weights
	bool bAllSum = true;
	for (int32 Vertex = 0; Vertex < A.Size * A.Size && bAllSum; Vertex += 7)
	{
		int32 Sum = 0;
		for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
		{
			Sum += A.Weights[Layer][Vertex];
		}
		bAllSum = Sum == 255;
	}
	TestTrue(TEXT("Layer weights add up to 255"), bAllSum);
	const int32 PadVertex = A.Index(FMath::RoundToInt(Pad.Centre.X + A.HalfExtent), FMath::RoundToInt(A.HalfExtent - Pad.Centre.Y));
	TestTrue(TEXT("The pad is mostly dirt"), A.Weights[BeyondTerrainLayers::Dirt][PadVertex] > 150);
	const FVector2D OnRoad(-60, -100);
	const int32 RoadVertex = A.Index(FMath::RoundToInt(OnRoad.X + A.HalfExtent), FMath::RoundToInt(A.HalfExtent - OnRoad.Y));
	TestTrue(TEXT("The road is path"), A.Weights[BeyondTerrainLayers::Path][RoadVertex] > 150 && A.RoadMask[RoadVertex] > 150);
	const FVector2D EastGround(180, -180);
	const int32 EastVertex = A.Index(FMath::RoundToInt(EastGround.X + A.HalfExtent), FMath::RoundToInt(A.HalfExtent - EastGround.Y));
	TestTrue(TEXT("The frost region is snowy"), A.Weights[BeyondTerrainLayers::Snow][EastVertex] > 100 || A.Weights[BeyondTerrainLayers::Rock][EastVertex] > 100);

	// The ridge and its pass
	const float OnRidge = A.SampleHeight(FVector2D(0, 120));
	const float BesideRidge = A.SampleHeight(FVector2D(-60, 120));
	const float InPass = A.SampleHeight(FVector2D(0, -100));
	TestTrue(*FString::Printf(TEXT("The ridge rises (%.0f m over %.0f m beside it)"), OnRidge, BesideRidge), OnRidge > BesideRidge + 15.0f);
	TestTrue(*FString::Printf(TEXT("The pass cuts through it (%.0f m)"), InPass), InPass < OnRidge - 15.0f);

	// The volcano
	const FBeyondLayoutVolcano& Volcano = Layout->Volcano;
	const float Crater = A.SampleHeight(Volcano.Centre);
	const float RimNorth = A.SampleHeight(Volcano.Centre + FVector2D(0.0, Volcano.RimRadius));
	TestTrue(*FString::Printf(TEXT("The crater (%.0f m) is below the rim (%.0f m)"), Crater, RimNorth), Crater < RimNorth - 10.0f);

	// The landscape conversion
	const TArray<uint16> Heights = A.ToLandscapeHeights(200.0f);
	const int32 Middle = A.Index(A.Size / 2, A.Size / 2);
	const float BackToMetres = (static_cast<float>(Heights[Middle]) - 32768.0f) * 200.0f / 128.0f / 100.0f;
	TestTrue(TEXT("Landscape heights convert back within 2 cm"), FMath::Abs(BackToMetres - A.Heights[Middle]) < 0.02f);

	// Plan 5D: the region mask (red frost, green molten, blue blight), north row first, laid over the landscape
	const int32 MaskSize = 128;
	UTexture2D* Mask = UBeyondGroundLibrary::BakeRegionMask(Layout, TEXT("/Temp/BeyondWorldBuilderTest/T_RegionMaskTest"), MaskSize);
	TArray64<uint8> MaskBytes;
	if (TestNotNull(TEXT("The region mask bakes"), Mask) && Mask->Source.GetMipData(MaskBytes, 0) && MaskBytes.Num() == MaskSize * MaskSize * 4)
	{
		// The pixel over a point (metres east / north), BGRA
		auto PixelAt = [&](const FVector2D& EastNorth)
		{
			const float Span = 2.0f * A.HalfExtent;
			const int32 PX = FMath::Clamp(FMath::FloorToInt((EastNorth.X + A.HalfExtent) / Span * MaskSize), 0, MaskSize - 1);
			const int32 PY = FMath::Clamp(FMath::FloorToInt((A.HalfExtent - EastNorth.Y) / Span * MaskSize), 0, MaskSize - 1);
			const uint8* Pixel = &MaskBytes[(PY * MaskSize + PX) * 4];
			return FColor(Pixel[2], Pixel[1], Pixel[0], Pixel[3]);
		};
		const FColor Frost = PixelAt(FVector2D(180, -180));
		const FColor Forest = PixelAt(FVector2D(-180, -180));
		TestTrue(*FString::Printf(TEXT("The frost region reads frost (red %d)"), Frost.R), Frost.R > 200 && Frost.G < 20 && Frost.B < 20);
		TestTrue(*FString::Printf(TEXT("The forest reads none of the three (%d, %d, %d)"), Forest.R, Forest.G, Forest.B),
			Forest.R < 20 && Forest.G < 20 && Forest.B < 20);
		bool bAtMostOne = true;
		for (int64 Index = 0; Index < MaskBytes.Num() && bAtMostOne; Index += 4 * 7)
		{
			bAtMostOne = MaskBytes[Index] + MaskBytes[Index + 1] + MaskBytes[Index + 2] <= 258;
		}
		TestTrue(TEXT("The mask's channels add up to at most 1"), bAtMostOne);
	}

	TArray<FName> LayerNames;
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
	{
		LayerNames.Add(BeyondTerrainLayers::GetName(Layer));
	}
	TestTrue(TEXT("The ground library has the landscape's 11 layers in order"), UBeyondGroundLibrary::GetLayerNames() == LayerNames);

	CheckFlora(*this, Layout, A);

	const TCHAR* GroundMaterialPath = TEXT("/Game/WorldsBeyond/World/Landscape/M_Beyond_Landscape");
	if (!FPackageName::DoesPackageExist(GroundMaterialPath))
	{
		AddWarning(TEXT("M_Beyond_Landscape isn't built yet (run migrate_pass18.py)"));
		return true;
	}
	const UMaterial* Ground = LoadObject<UMaterial>(nullptr, TEXT("/Game/WorldsBeyond/World/Landscape/M_Beyond_Landscape.M_Beyond_Landscape"));
	if (!TestNotNull(TEXT("M_Beyond_Landscape loads"), Ground))
	{
		return true;
	}
	int32 Blends = 0;
	int32 GrassTypes = 0;
	for (const TObjectPtr<UMaterialExpression>& Expression : Ground->GetExpressions())
	{
		if (const UMaterialExpressionLandscapeLayerBlend* Blend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expression))
		{
			++Blends;
			TArray<FName> Names;
			bool bHeight = true;
			for (const FLayerBlendInput& Input : Blend->Layers)
			{
				Names.Add(Input.LayerName);
				bHeight &= Input.BlendType == LB_HeightBlend && Input.HeightInput.Expression != nullptr;
			}
			TestTrue(TEXT("A layer blend lists the 11 layers in order"), Names == LayerNames);
			TestTrue(TEXT("...each height blended with a height input"), bHeight);
		}
		else if (const UMaterialExpressionLandscapeGrassOutput* Grass = Cast<UMaterialExpressionLandscapeGrassOutput>(Expression))
		{
			for (const FGrassInput& Input : Grass->GrassTypes)
			{
				GrassTypes += Input.GrassType && Input.Input.Expression ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("Three layer blends (colour, normal, roughness)"), Blends, 3);
	TestEqual(TEXT("Six grass types, each fed by its layer"), GrassTypes, 6);
	return true;
}

#endif
