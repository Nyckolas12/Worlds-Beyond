// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BeyondRegionDefinition.generated.h"

class UFXSystemAsset;
class USoundBase;

UENUM(BlueprintType)
enum class EBeyondRegionKind : uint8
{
	// A whole region (the Elderwood, the Blightwood...): name banner, level band, weather, music
	Region,
	// A village inside a region: its own banner and banter, the region's weather and music unless it overrides them
	Village,
	// A named spot that isn't a village (a crossroads, a pass)
	Area
};

/** How a region looks: blended in over a few seconds when the party walks in (ABeyondWorldInfo) */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondWeatherPreset
{
	GENERATED_BODY()

	// The sun (directional light), lux
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0"))
	float SunIntensity = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FLinearColor SunColor = FLinearColor(1.0f, 0.95f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0"))
	float SkyLightIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FLinearColor SkyLightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0"))
	float FogDensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.001"))
	float FogHeightFalloff = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FLinearColor FogColor = FLinearColor(0.45f, 0.55f, 0.7f);

	// Colour grading: 1 keeps the colours, below 1 washes them out (the corrupted woods)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "2"))
	float Saturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float ExposureBias = 0.0f;

	// Snow, ash, embers, spores... follows the camera while the party is here (Niagara or Cascade; looping)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TSoftObjectPtr<UFXSystemAsset> Precipitation;

	FBeyondWeatherPreset Lerp(const FBeyondWeatherPreset& To, float Alpha) const;
};

/**
 * A region, village or area of the open world (Plan 5): name, banner, level band, discovery reward, music, ambience,
 * weather and map colour. Region volumes (ABeyondRegionVolume) point at one; the world subsystem tracks which one the
 * party leader stands in. Made by migrate_pass14.py in /Game/WorldsBeyond/World/Regions (names in world_content.py).
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondRegionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Also the banter context unless Banter Context says otherwise (region_forest, village_frostholm...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FName RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FText DisplayName;

	// The small line over the name on the banner ("The Wandering Dominion", "Village")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	EBeyondRegionKind Kind = EBeyondRegionKind::Region;

	// The region a village or area sits in (its Region Id)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FName ParentId;

	// The enemies' region tag (Region.Forest...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FGameplayTag RegionTag;

	// Enemy levels here: spawners with Level 0 use the band (plus their Level Offset); shown on the banner
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Level", meta = (ClampMin = "1"))
	int32 LevelMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Level", meta = (ClampMin = "1"))
	int32 LevelMax = 5;

	// DT_Dialogue banter lines with this context play when the party walks in (empty: the Region Id)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FName BanterContext;

	// EXP for both demigods the first time they walk in (0: Project Settings -> Worlds Beyond World)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region", meta = (ClampMin = "0"))
	float DiscoveryExp = 0.0f;

	// Looping track; a village without one keeps its region's
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Sound")
	TSoftObjectPtr<USoundBase> Music;

	// Looping ambience bed; a village without one keeps its region's
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Sound")
	TSoftObjectPtr<USoundBase> Ambience;

	// Use Weather here (regions always do; villages and areas keep their region's unless this is on)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Weather")
	bool bOverrideWeather = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Weather", meta = (EditCondition = "bOverrideWeather"))
	FBeyondWeatherPreset Weather;

	// Colour on the world map (region tint, village dots)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region|Map")
	FLinearColor MapColour = FLinearColor(0.35f, 0.6f, 0.3f);

	UFUNCTION(BlueprintPure, Category = "Region")
	FName GetBanterContext() const { return BanterContext.IsNone() ? RegionId : BanterContext; }

	// "Lv 1-6"
	UFUNCTION(BlueprintPure, Category = "Region")
	FText GetLevelText() const;

	UFUNCTION(BlueprintPure, Category = "Region")
	FText GetDisplayNameOrId() const;
};
