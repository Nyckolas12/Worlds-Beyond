// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BeyondUITypes.generated.h"

/** The menu screens (the character menu's sidebar pages, the map and the pause menu) */
UENUM(BlueprintType)
enum class EBeyondMenuPage : uint8
{
	SkillTree,
	Armor,
	Overview,
	Quests,
	WorldMap,
	Pause
};
