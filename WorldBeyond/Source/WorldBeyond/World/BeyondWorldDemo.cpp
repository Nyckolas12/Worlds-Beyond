// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondWorldDemo.h"
#include "WorldBeyond.h"
#include "BeyondGameplayTags.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "World/BeyondPointOfInterest.h"
#include "World/BeyondRegionVolume.h"
#include "World/BeyondWaystone.h"
#include "World/BeyondWorldInfo.h"

#define LOCTEXT_NAMESPACE "BeyondWorldDemo"

namespace BeyondWorldDemo
{
	TArray<TWeakObjectPtr<AActor>> FDemoActors::All() const
	{
		return { WorldInfo, Region, Village, Near, Far, Place, Lava, DeepWater };
	}

	UBeyondRegionDefinition* MakeRegion(FName RegionId, const FText& DisplayName, EBeyondRegionKind Kind, int32 LevelMin, int32 LevelMax, FName ParentId)
	{
		UBeyondRegionDefinition* Region = NewObject<UBeyondRegionDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Region->RegionId = RegionId;
		Region->DisplayName = DisplayName;
		Region->Kind = Kind;
		Region->LevelMin = LevelMin;
		Region->LevelMax = LevelMax;
		Region->ParentId = ParentId;
		Region->bOverrideWeather = Kind == EBeyondRegionKind::Region;
		return Region;
	}

	ABeyondWorldInfo* SpawnWorldInfo(UWorld* World, const FVector& Location)
	{
		return World ? World->SpawnActor<ABeyondWorldInfo>(ABeyondWorldInfo::StaticClass(), FTransform(Location)) : nullptr;
	}

	ABeyondRegionVolume* SpawnRegionVolume(UWorld* World, UBeyondRegionDefinition* Region, const FVector& Centre, float Radius, int32 Priority)
	{
		ABeyondRegionVolume* Volume = World ? World->SpawnActorDeferred<ABeyondRegionVolume>(ABeyondRegionVolume::StaticClass(), FTransform(Centre)) : nullptr;
		if (Volume)
		{
			Volume->Region = Region;
			Volume->Radius = Radius;
			Volume->Priority = Priority;
			Volume->FinishSpawning(FTransform(Centre));
		}
		return Volume;
	}

	ABeyondWaystone* SpawnWaystone(UWorld* World, FName WaystoneId, const FText& DisplayName, const FVector& GroundLocation, float Yaw, bool bStartAttuned)
	{
		const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), GroundLocation);
		ABeyondWaystone* Waystone = World ? World->SpawnActorDeferred<ABeyondWaystone>(ABeyondWaystone::StaticClass(), Transform) : nullptr;
		if (Waystone)
		{
			Waystone->WaystoneId = WaystoneId;
			Waystone->DisplayName = DisplayName;
			Waystone->bStartAttuned = bStartAttuned;
			Waystone->DiscoveryRadius = 1500.0f;
			Waystone->FinishSpawning(Transform);
		}
		return Waystone;
	}

	ABeyondPointOfInterest* SpawnPlace(UWorld* World, FName PoiId, const FText& DisplayName, const FVector& Location, float DiscoveryRadius)
	{
		ABeyondPointOfInterest* Place = World ? World->SpawnActorDeferred<ABeyondPointOfInterest>(ABeyondPointOfInterest::StaticClass(), FTransform(Location)) : nullptr;
		if (Place)
		{
			Place->PoiId = PoiId;
			Place->DisplayName = DisplayName;
			Place->Kind = EBeyondPoiKind::Shrine;
			Place->DiscoveryRadius = DiscoveryRadius;
			Place->FinishSpawning(FTransform(Location));
		}
		return Place;
	}

	ABeyondHazardVolume* SpawnHazard(UWorld* World, EBeyondHazardKind Kind, const FVector& Centre, const FVector& Extent)
	{
		ABeyondHazardVolume* Hazard = World ? World->SpawnActorDeferred<ABeyondHazardVolume>(ABeyondHazardVolume::StaticClass(), FTransform(Centre)) : nullptr;
		if (Hazard)
		{
			Hazard->Kind = Kind;
			Hazard->Box->SetBoxExtent(Extent);
			Hazard->FinishSpawning(FTransform(Centre));
		}
		return Hazard;
	}

	FDemoActors Spawn(UWorld* World, const FVector& GroundOrigin, const FVector& Forward, bool bTransientDefinitions)
	{
		FDemoActors Demo;
		if (!World)
		{
			return Demo;
		}
		const FVector Ahead = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal2D();
		const FVector Right(-Ahead.Y, Ahead.X, 0.0f);
		const float Yaw = Ahead.Rotation().Yaw;

		UBeyondRegionDefinition* Forest = bTransientDefinitions ? nullptr
			: LoadObject<UBeyondRegionDefinition>(nullptr, TEXT("/Game/WorldsBeyond/World/Regions/DA_Region_region_forest.DA_Region_region_forest"), nullptr, LOAD_NoWarn);
		UBeyondRegionDefinition* Village = bTransientDefinitions ? nullptr
			: LoadObject<UBeyondRegionDefinition>(nullptr, TEXT("/Game/WorldsBeyond/World/Regions/DA_Region_village_mossbrook.DA_Region_village_mossbrook"), nullptr, LOAD_NoWarn);
		if (!Forest)
		{
			Forest = MakeRegion(TEXT("region_forest"), LOCTEXT("Forest", "The Elderwood"), EBeyondRegionKind::Region, 1, 6);
			Forest->RegionTag = BeyondTags::Region_Forest;
			Forest->Subtitle = LOCTEXT("Dominion", "The Wandering Dominion");
			Forest->Weather.FogDensity = 0.03f;
			Forest->Weather.FogColor = FLinearColor(0.5f, 0.6f, 0.45f);
		}
		if (!Village)
		{
			Village = MakeRegion(TEXT("region_village"), LOCTEXT("Village", "Mossbrook"), EBeyondRegionKind::Village, 1, 6, Forest->RegionId);
			Village->MapColour = FLinearColor(1.0f, 0.8f, 0.4f);
		}

		Demo.WorldInfo = SpawnWorldInfo(World, GroundOrigin + FVector(0.0f, 0.0f, 200.0f));
		Demo.Region = SpawnRegionVolume(World, Forest, GroundOrigin + Ahead * 1800.0f, 2400.0f, 0);
		Demo.Village = SpawnRegionVolume(World, Village, GroundOrigin + Ahead * 2600.0f, 550.0f, 10);
		Demo.Near = SpawnWaystone(World, TEXT("demo_near"), LOCTEXT("NearStone", "Wayside Stone"), GroundOrigin + Right * 450.0f, Yaw, false);
		Demo.Far = SpawnWaystone(World, TEXT("demo_far"), LOCTEXT("FarStone", "Hilltop Stone"), GroundOrigin + Ahead * 3600.0f, Yaw + 180.0f, false);
		Demo.Place = SpawnPlace(World, TEXT("demo_shrine"), LOCTEXT("Shrine", "Mossy Shrine"), GroundOrigin + Ahead * 1500.0f + Right * 900.0f, 500.0f);
		Demo.Lava = SpawnHazard(World, EBeyondHazardKind::Burn, GroundOrigin + Ahead * 1200.0f - Right * 800.0f + FVector(0.0f, 0.0f, 100.0f), FVector(200.0f, 200.0f, 150.0f));
		Demo.DeepWater = SpawnHazard(World, EBeyondHazardKind::DeepWater, GroundOrigin + Ahead * 1200.0f - Right * 1400.0f + FVector(0.0f, 0.0f, 100.0f),
			FVector(200.0f, 200.0f, 150.0f));
		UE_LOG(LogBeyond, Display, TEXT("World demo: region and village ahead, a waystone to your right (F attunes, F again rests), another far ahead, "
			"a shrine ahead right, lava ahead left, deep water beyond it. M opens the map."));
		return Demo;
	}
}

#undef LOCTEXT_NAMESPACE
