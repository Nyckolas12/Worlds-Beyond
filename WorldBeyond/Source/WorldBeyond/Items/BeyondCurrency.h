// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BeyondCurrency.generated.h"

/** The party's money (Plan 6): kept in the bag's wallet, shown on the menus */
UENUM(BlueprintType)
enum class EBeyondCurrency : uint8
{
	// Kills, salvage, quests; pays for upgrades
	Gold,
	// Elites, bosses, discovery, chapters; upgrades from +5
	Crystals,
	// Places, waystones, salvaging rare gear; upgrades from +10
	EchoShards,
	// Main chapters and story bosses
	ReverieFragments
};
