// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Progression/BeyondSkillTree.h"
#include "UI/BeyondUIGlyphs.h"

class ABeyondCharacterBase;

namespace BeyondUI
{
	class FPainter;
}

/** One line of a character's combat stats as the menus show it */
struct FBeyondStatRow
{
	EBeyondSkillStat Stat = EBeyondSkillStat::MaxHealth;
	EBeyondGlyph Glyph = EBeyondGlyph::None;
	FText Name;
	float Value = 0.0f;
	// From equipment (shown green)
	float Bonus = 0.0f;
	bool bPercent = false;
};

/** Level and EXP into it */
struct FBeyondLevelInfo
{
	int32 Level = 1;
	float Experience = 0.0f;
	// 0 at the max level
	float ExperienceToNext = 0.0f;
};

/** Panels several menu pages share (Plan 6) */
namespace BeyondMenuPanels
{
	// Stats in the order the menus list them
	WORLDBEYOND_API const TArray<EBeyondSkillStat>& GetMenuStats();
	WORLDBEYOND_API EBeyondGlyph GetStatGlyph(EBeyondSkillStat Stat);
	WORLDBEYOND_API TArray<FBeyondStatRow> GetCombatStats(const ABeyondCharacterBase* Character);
	WORLDBEYOND_API FBeyondLevelInfo GetLevelInfo(const ABeyondCharacterBase* Character);
	// "1,713" / "18%"
	WORLDBEYOND_API FString FormatStatValue(const FBeyondStatRow& Row);
	// "+137" / "+3%" (empty without a bonus)
	WORLDBEYOND_API FString FormatStatBonus(const FBeyondStatRow& Row);

	// "LEVEL 50   12,480 / 23,000" over a gold bar; returns the height used
	WORLDBEYOND_API float PaintLevel(const BeyondUI::FPainter& Paint, const FBeyondLevelInfo& Info, const FVector2f& TopLeft, float Width, int32 Layer,
		const FText& Label = FText::GetEmpty());
	// Icon, name, value and the gear bonus per row; returns the height used
	WORLDBEYOND_API float PaintStats(const BeyondUI::FPainter& Paint, const TArray<FBeyondStatRow>& Rows, const FVector2f& TopLeft, float Width, float RowHeight,
		int32 Layer, bool bShowBonus = true, float FontSize = 19.0f);
}
