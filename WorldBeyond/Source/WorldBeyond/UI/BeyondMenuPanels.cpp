// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondMenuPanels.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Progression/BeyondStatEffects.h"
#include "UI/BeyondUIStyle.h"

#define LOCTEXT_NAMESPACE "BeyondMenuPanels"

const TArray<EBeyondSkillStat>& BeyondMenuPanels::GetMenuStats()
{
	static const TArray<EBeyondSkillStat> Stats = { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength,
		EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense, EBeyondSkillStat::CritRate, EBeyondSkillStat::CritDamage, EBeyondSkillStat::ElementalMastery };
	return Stats;
}

EBeyondGlyph BeyondMenuPanels::GetStatGlyph(EBeyondSkillStat Stat)
{
	switch (Stat)
	{
	case EBeyondSkillStat::MaxHealth: return EBeyondGlyph::Heart;
	case EBeyondSkillStat::MaxStamina: return EBeyondGlyph::Stamina;
	case EBeyondSkillStat::Strength: return EBeyondGlyph::Attack;
	case EBeyondSkillStat::Arcana: return EBeyondGlyph::Magic;
	case EBeyondSkillStat::CritRate: return EBeyondGlyph::Crit;
	case EBeyondSkillStat::CritDamage: return EBeyondGlyph::CritDamage;
	case EBeyondSkillStat::ElementalMastery: return EBeyondGlyph::Mastery;
	case EBeyondSkillStat::VerdantAffinity: return EBeyondGlyph::Verdant;
	case EBeyondSkillStat::ShadowAffinity: return EBeyondGlyph::Shadow;
	case EBeyondSkillStat::FrostAffinity: return EBeyondGlyph::Frost;
	case EBeyondSkillStat::FlameAffinity: return EBeyondGlyph::Flame;
	default: return EBeyondGlyph::Shield;
	}
}

TArray<FBeyondStatRow> BeyondMenuPanels::GetCombatStats(const ABeyondCharacterBase* Character)
{
	TArray<FBeyondStatRow> Rows;
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const UBeyondEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	for (const EBeyondSkillStat Stat : GetMenuStats())
	{
		FBeyondStatRow& Row = Rows.AddDefaulted_GetRef();
		Row.Stat = Stat;
		Row.Glyph = GetStatGlyph(Stat);
		Row.Name = UBeyondSkillTreeAsset::GetStatName(Stat);
		const FGameplayAttribute Attribute = BeyondStats::GetStatAttribute(Stat);
		Row.Value = ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : 0.0f;
		Row.Bonus = Equipment ? Equipment->GetStatBonus(Stat) : 0.0f;
		Row.bPercent = BeyondStats::IsPercentStat(Stat);
		// Critical damage reads as the whole hit: 50 extra is "150%"
		if (Stat == EBeyondSkillStat::CritDamage)
		{
			Row.Value += 100.0f;
		}
	}
	return Rows;
}

FBeyondLevelInfo BeyondMenuPanels::GetLevelInfo(const ABeyondCharacterBase* Character)
{
	FBeyondLevelInfo Info;
	if (Character)
	{
		Info.Level = Character->GetCharacterLevel();
		Info.Experience = Character->GetExperience();
		Info.ExperienceToNext = Character->GetExperienceToNextLevel();
	}
	return Info;
}

FString BeyondMenuPanels::FormatStatValue(const FBeyondStatRow& Row)
{
	return Row.bPercent ? BeyondUI::FormatPercent(Row.Value) : BeyondUI::FormatNumber(FMath::RoundToInt64(Row.Value));
}

FString BeyondMenuPanels::FormatStatBonus(const FBeyondStatRow& Row)
{
	if (FMath::Abs(Row.Bonus) < 0.5f)
	{
		return FString();
	}
	const FString Number = Row.bPercent ? BeyondUI::FormatPercent(FMath::Abs(Row.Bonus)) : BeyondUI::FormatNumber(FMath::RoundToInt64(FMath::Abs(Row.Bonus)));
	return (Row.Bonus > 0.0f ? TEXT("+") : TEXT("-")) + Number;
}

float BeyondMenuPanels::PaintLevel(const BeyondUI::FPainter& Paint, const FBeyondLevelInfo& Info, const FVector2f& TopLeft, float Width, int32 Layer,
	const FText& Label)
{
	using namespace BeyondUI;
	const FPalette& P = Palette();
	const FString LabelText = Label.IsEmpty() ? LOCTEXT("Level", "Level").ToString() : Label.ToString();
	const FVector2f LabelSize = Paint.Text(LabelText, Font(EBeyondFont::Heading, 18.0f), TopLeft + FVector2f(0.0f, 12.0f), P.Text, 0.0f, Layer);
	Paint.Text(FString::FromInt(Info.Level), Font(EBeyondFont::Number, 36.0f), TopLeft + FVector2f(LabelSize.X + 12.0f, 0.0f), P.Text, 0.0f, Layer);
	if (Info.ExperienceToNext < 0.0f)
	{
		// No EXP to show (the duo's party level)
		return 54.0f;
	}
	const bool bMax = Info.ExperienceToNext <= 0.0f;
	const FString Progress = bMax ? LOCTEXT("MaxLevel", "Max level").ToString()
		: FString::Printf(TEXT("%s / %s"), *FormatNumber(FMath::FloorToInt64(Info.Experience)), *FormatNumber(FMath::RoundToInt64(Info.ExperienceToNext)));
	Paint.Text(Progress, Font(EBeyondFont::Number, 17.0f), TopLeft + FVector2f(Width, 16.0f), P.Text, 1.0f, Layer);
	const float Fraction = bMax ? 1.0f : Info.Experience / FMath::Max(Info.ExperienceToNext, 1.0f);
	Paint.Bar(TopLeft + FVector2f(12.0f, 56.0f), FVector2f(Width - 24.0f, 7.0f), Fraction, P.GoldBright, Layer);
	return 74.0f;
}

float BeyondMenuPanels::PaintStats(const BeyondUI::FPainter& Paint, const TArray<FBeyondStatRow>& Rows, const FVector2f& TopLeft, float Width, float RowHeight,
	int32 Layer, bool bShowBonus, float FontSize)
{
	using namespace BeyondUI;
	const FPalette& P = Palette();
	const FSlateFontInfo NameFont = Font(EBeyondFont::Body, FontSize);
	const FSlateFontInfo ValueFont = Font(EBeyondFont::Number, FontSize + 1.0f);
	const FSlateFontInfo BonusFont = Font(EBeyondFont::Body, FontSize - 1.0f);
	const float ValueRight = TopLeft.X + Width - (bShowBonus ? 74.0f : 4.0f);
	float Y = TopLeft.Y;
	for (const FBeyondStatRow& Row : Rows)
	{
		const float TextY = Y + (RowHeight - MeasureText(TEXT("Hg"), NameFont).Y) * 0.5f;
		Paint.Glyph(Row.Glyph, FVector2f(TopLeft.X + 12.0f, Y + RowHeight * 0.5f), 22.0f, P.Text, Layer, 1.5f);
		Paint.Text(Row.Name.ToString(), NameFont, FVector2f(TopLeft.X + 40.0f, TextY), P.Text, 0.0f, Layer);
		Paint.Text(FormatStatValue(Row), ValueFont, FVector2f(ValueRight, TextY - 1.0f), P.Text, 1.0f, Layer);
		if (bShowBonus)
		{
			Paint.Text(FormatStatBonus(Row), BonusFont, FVector2f(ValueRight + 16.0f, TextY + 1.0f), P.Plus, 0.0f, Layer);
		}
		Y += RowHeight;
	}
	return Y - TopLeft.Y;
}

#undef LOCTEXT_NAMESPACE
