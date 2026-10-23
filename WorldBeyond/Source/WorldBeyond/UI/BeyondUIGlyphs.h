// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/** Line icons drawn in code (stats, slots, sidebar entries, elements, small badges) for everything without a picture */
enum class EBeyondGlyph : uint8
{
	None,
	// Equipment slots
	Helm,
	Chest,
	Gauntlets,
	Boots,
	Sword,
	Staff,
	Ring,
	Amulet,
	// Stats
	Heart,
	Stamina,
	Attack,
	Magic,
	Shield,
	Crit,
	CritDamage,
	Mastery,
	// Currencies and points
	Coin,
	Crystal,
	Shard,
	Fragment,
	SkillPoint,
	BondPoint,
	// Menu pages
	SkillTree,
	Armor,
	Overview,
	Quests,
	Map,
	// Badges and filters
	Lock,
	Grid,
	Check,
	Cross,
	Star,
	Up,
	Bond,
	Hourglass,
	Compass,
	Skull,
	// Elements
	Storm,
	Radiant,
	Eclipse,
	Verdant,
	Shadow,
	Frost,
	Flame
};

namespace BeyondUI
{
	/** The glyph's strokes in a 1 x 1 box around (0, 0); y points down */
	WORLDBEYOND_API TArray<TArray<FVector2f>> GetGlyphStrokes(EBeyondGlyph Glyph);
}
