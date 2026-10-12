// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondFloraTile.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

const FName ABeyondFloraTile::ColliderTag(TEXT("BeyondTrunkColliders"));

ABeyondFloraTile::ABeyondFloraTile()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

int32 ABeyondFloraTile::GetPlantCount() const
{
	TArray<UInstancedSkinnedMeshComponent*> Plants;
	GetPlantComponents(Plants);
	int32 Count = 0;
	for (const UInstancedSkinnedMeshComponent* Plant : Plants)
	{
		Count += Plant->GetInstanceCount();
	}
	return Count;
}

int32 ABeyondFloraTile::GetColliderCount() const
{
	const UInstancedStaticMeshComponent* Colliders = GetColliders();
	return Colliders ? Colliders->GetInstanceCount() : 0;
}

void ABeyondFloraTile::GetPlantComponents(TArray<UInstancedSkinnedMeshComponent*>& Out) const
{
	GetComponents<UInstancedSkinnedMeshComponent>(Out);
}

UInstancedStaticMeshComponent* ABeyondFloraTile::GetColliders() const
{
	TArray<UInstancedStaticMeshComponent*> Meshes;
	GetComponents<UInstancedStaticMeshComponent>(Meshes);
	for (UInstancedStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(ColliderTag))
		{
			return Mesh;
		}
	}
	return nullptr;
}
