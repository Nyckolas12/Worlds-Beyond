// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BeyondOpenWorldSettings.generated.h"

/**
 * The open world (Project Settings -> Game -> Worlds Beyond World): how often the party's whereabouts are checked,
 * discovery rewards, the region banner, fast travel and the streaming hold, navigation around the demigods, weather and
 * music blending. Saved in Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond World"))
class WORLDBEYOND_API UBeyondOpenWorldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Seconds between checks of where the leader is (regions, places, waystones, safe ground)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "World", meta = (ClampMin = "0.05"))
	float ScanInterval = 0.25f;

	//~ Discovery

	// EXP for both demigods the first time they enter a region / village (a definition's own Discovery Exp wins)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Discovery", meta = (ClampMin = "0"))
	float RegionDiscoveryExp = 120.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Discovery", meta = (ClampMin = "0"))
	float VillageDiscoveryExp = 40.0f;

	// EXP for finding a place (cave, shrine, vista...)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Discovery", meta = (ClampMin = "0"))
	float PlaceDiscoveryExp = 30.0f;

	//~ Banner

	// How long the region name stays on screen
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banner", meta = (ClampMin = "0.5"))
	float BannerHoldTime = 3.2f;

	// The same region's banner doesn't show again within this many seconds (walking back and forth across a border)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banner", meta = (ClampMin = "0"))
	float BannerRepeatCooldown = 30.0f;

	//~ Fast travel and the streaming hold

	// No fast travel while an enemy this close to a demigod is fighting
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0"))
	float FastTravelCombatRadius = 3500.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0"))
	float FadeOutTime = 0.5f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0"))
	float FadeInTime = 0.7f;

	// Give up waiting for the destination to stream in after this long (the party goes to the player start)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "1"))
	float StreamingTimeout = 20.0f;

	// After the ground is there, wait at most this long for the navigation mesh around the party
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Travel", meta = (ClampMin = "0"))
	float NavigationWait = 3.0f;

	//~ Navigation (open-world maps generate it only around the demigods)

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "1000"))
	float InvokerRadius = 7000.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "1000"))
	float InvokerRemovalRadius = 9000.0f;

	//~ Weather and sound

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "0"))
	float WeatherBlendTime = 6.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0"))
	float MusicFadeTime = 3.0f;
};
