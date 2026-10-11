// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "World/BeyondHazardVolume.h"
#include "World/BeyondRegionDefinition.h"

class ABeyondPointOfInterest;
class ABeyondRegionVolume;
class ABeyondWaystone;
class ABeyondWorldInfo;

/**
 * Builds Plan 5 actors at runtime: the console's Beyond.WorldDemo (a small open-world setup next to the leader, in any
 * map) and the World test use it. Region definitions made here are transient (kept alive by their volumes).
 */
namespace BeyondWorldDemo
{
	struct FDemoActors
	{
		TWeakObjectPtr<ABeyondWorldInfo> WorldInfo;
		TWeakObjectPtr<ABeyondRegionVolume> Region;
		TWeakObjectPtr<ABeyondRegionVolume> Village;
		TWeakObjectPtr<ABeyondWaystone> Near;
		TWeakObjectPtr<ABeyondWaystone> Far;
		TWeakObjectPtr<ABeyondPointOfInterest> Place;
		TWeakObjectPtr<ABeyondHazardVolume> Lava;
		TWeakObjectPtr<ABeyondHazardVolume> DeepWater;

		TArray<TWeakObjectPtr<AActor>> All() const;
	};

	WORLDBEYOND_API UBeyondRegionDefinition* MakeRegion(FName RegionId, const FText& DisplayName, EBeyondRegionKind Kind, int32 LevelMin, int32 LevelMax,
		FName ParentId = NAME_None);
	WORLDBEYOND_API ABeyondWorldInfo* SpawnWorldInfo(UWorld* World, const FVector& Location);
	WORLDBEYOND_API ABeyondRegionVolume* SpawnRegionVolume(UWorld* World, UBeyondRegionDefinition* Region, const FVector& Centre, float Radius, int32 Priority);
	WORLDBEYOND_API ABeyondWaystone* SpawnWaystone(UWorld* World, FName WaystoneId, const FText& DisplayName, const FVector& GroundLocation, float Yaw,
		bool bStartAttuned = false);
	WORLDBEYOND_API ABeyondPointOfInterest* SpawnPlace(UWorld* World, FName PoiId, const FText& DisplayName, const FVector& Location, float DiscoveryRadius);
	WORLDBEYOND_API ABeyondHazardVolume* SpawnHazard(UWorld* World, EBeyondHazardKind Kind, const FVector& Centre, const FVector& Extent);

	// The console demo: a region around the spot ahead, a village in it, a waystone beside the leader and one far
	// ahead, a place, a lava pool and deep water. Uses the real forest / Mossbrook definitions when pass 14 made them.
	WORLDBEYOND_API FDemoActors Spawn(UWorld* World, const FVector& GroundOrigin, const FVector& Forward, bool bTransientDefinitions);
}
