// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeyondFloraTile.generated.h"

class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * The Megaplants of one 256 m square of the open world (Plan 5E, pass 19): elders, Baltic pine saplings, dead pines,
 * heather and reeds are skeletal meshes swaying in the dynamic wind, which the Foliage tool can't hold, so each tile
 * carries one instanced skinned mesh component per plant and, for the trees, an invisible instanced cylinder per trunk
 * (the Megaplants have no physics asset: these block the party and carve the navmesh). The components are added by
 * UBeyondFloraLibrary::PlaceSkinnedFlora as instance components; the static trees and rocks are in the World Partition
 * foliage instead.
 */
UCLASS()
class WORLDBEYOND_API ABeyondFloraTile : public AActor
{
	GENERATED_BODY()

public:
	ABeyondFloraTile();

	// Plants drawn in this tile (all meshes)
	UFUNCTION(BlueprintPure, Category = "Flora")
	int32 GetPlantCount() const;

	// Trunks that block (the invisible colliders)
	UFUNCTION(BlueprintPure, Category = "Flora")
	int32 GetColliderCount() const;

	// The plant components, one per mesh
	void GetPlantComponents(TArray<UInstancedSkinnedMeshComponent*>& Out) const;

	// The trunk colliders (nullptr when the tile holds no trees)
	UInstancedStaticMeshComponent* GetColliders() const;

	// Component tag of the trunk colliders
	static const FName ColliderTag;
};
