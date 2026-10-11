// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondRegionVolume.h"
#include "Components/BillboardComponent.h"
#include "Components/SplineComponent.h"
#include "World/BeyondWorldSubsystem.h"

ABeyondRegionVolume::ABeyondRegionVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	// Logic for the whole map: never streamed out
	bIsSpatiallyLoaded = false;

	Outline = CreateDefaultSubobject<USplineComponent>(TEXT("Outline"));
	SetRootComponent(Outline);
	Outline->SetClosedLoop(true);
	Outline->ClearSplinePoints(true);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Outline);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

void ABeyondRegionVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildPolygon();
}

void ABeyondRegionVolume::BeginPlay()
{
	Super::BeginPlay();
	RebuildPolygon();
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->RegisterRegionVolume(this);
	}
}

void ABeyondRegionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondWorldSubsystem* WorldSubsystem = UBeyondWorldSubsystem::Get(this))
	{
		WorldSubsystem->UnregisterRegionVolume(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABeyondRegionVolume::SetOutline(const TArray<FVector>& WorldPoints)
{
	Outline->ClearSplinePoints(false);
	for (const FVector& Point : WorldPoints)
	{
		Outline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}
	for (int32 Index = 0; Index < Outline->GetNumberOfSplinePoints(); ++Index)
	{
		Outline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	Outline->SetClosedLoop(true, false);
	Outline->UpdateSpline();
	RebuildPolygon();
}

void ABeyondRegionVolume::RebuildPolygon()
{
	Polygon.Reset();
	Bounds = FBox2D(ForceInit);
	const int32 Count = Outline ? Outline->GetNumberOfSplinePoints() : 0;
	if (Count >= 3)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Point = Outline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
			Polygon.Add(FVector2D(Point.X, Point.Y));
		}
	}
	else
	{
		// A circle
		const FVector Centre = GetActorLocation();
		constexpr int32 Segments = 32;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle = 2.0f * PI * Index / Segments;
			Polygon.Add(FVector2D(Centre.X + FMath::Cos(Angle) * Radius, Centre.Y + FMath::Sin(Angle) * Radius));
		}
	}
	for (const FVector2D& Point : Polygon)
	{
		Bounds += Point;
	}
}

bool ABeyondRegionVolume::ContainsLocation(const FVector& Location) const
{
	const FVector2D Point(Location.X, Location.Y);
	if (Polygon.Num() < 3 || !Bounds.IsInside(Point))
	{
		return false;
	}
	// Even-odd rule
	bool bInside = false;
	for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[Previous];
		if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

FVector ABeyondRegionVolume::GetCentre() const
{
	if (Polygon.IsEmpty())
	{
		return GetActorLocation();
	}
	FVector2D Sum = FVector2D::ZeroVector;
	for (const FVector2D& Point : Polygon)
	{
		Sum += Point;
	}
	Sum /= Polygon.Num();
	return FVector(Sum.X, Sum.Y, GetActorLocation().Z);
}
