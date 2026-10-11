// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BeyondWorldMapData.generated.h"

class UTexture2D;

/**
 * The world map's picture (Plan 5): a texture covering World Min..World Max (X east, Y south, Unreal units), plus an
 * optional fog mask per region (white where the region is) that the map draws over regions not found yet. Baked from
 * the terrain by the world builder (DA_WorldMap); a painted map can replace the texture as long as it covers the same
 * rectangle.
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondWorldMapData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UTexture2D> MapTexture;

	// The world rectangle the texture shows (its top-left and bottom-right corners)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D WorldMin = FVector2D(-201600.0, -201600.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	FVector2D WorldMax = FVector2D(201600.0, 201600.0);

	// Region id -> mask over the same rectangle (white inside the region)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TMap<FName, TObjectPtr<UTexture2D>> RegionFogMasks;
};
