// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "World/BeyondHeightfieldGenerator.h"
#include "World/BeyondWorldLayout.h"

/**
 * Plan 5B's terrain generator on a small layout (no map, no PIE): the same seed gives the same terrain, pads are flat,
 * roads keep to the grade limit, lake floors sit under the water, layer weights add up to 255, ridges rise and their
 * passes cut through, the volcano's crater is lower than its rim.
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
	return true;
}

#endif
