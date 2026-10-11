// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "BeyondOpenWorldNavigationSystem.generated.h"

/**
 * The open world's navigation (Plan 5): the navmesh is only built around navigation invokers (the two demigods, which
 * the world subsystem registers), at runtime, so re-sculpting the terrain never needs a navigation rebuild. Set as the
 * Navigation System Class in an open-world map's World Settings (migrate_pass15.py does); other maps keep the project's
 * navigation system and their built navmesh. The map's RecastNavMesh needs Runtime Generation = Dynamic.
 */
UCLASS()
class WORLDBEYOND_API UBeyondOpenWorldNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()

public:
	UBeyondOpenWorldNavigationSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitProperties() override;
};
