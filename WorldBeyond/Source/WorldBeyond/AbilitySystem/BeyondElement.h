// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BeyondElement.generated.h"

/**
 * Elements (Plan 6): each demigod's own (Storm, Radiant, the duo's Eclipse) and the four the regions' foes carry
 * (Verdant forest, Shadow corrupted woods, Frost, Flame). Elemental Mastery boosts a demigod's own element;
 * affinity to a region element is bonus damage against its foes and resistance to its hits.
 */
UENUM(BlueprintType)
enum class EBeyondElement : uint8
{
	None,
	Storm,
	Radiant,
	Eclipse,
	Verdant,
	Shadow,
	Frost,
	Flame
};
