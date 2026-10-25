// Fill out your copyright notice in the Description page of Project Settings.

#include "Progression/BeyondProgressionSettings.h"

UBeyondProgressionSettings::UBeyondProgressionSettings()
{
	ExperienceByRank.Add(EBeyondEnemyRank::Regular, 25.0f);
	ExperienceByRank.Add(EBeyondEnemyRank::Elite, 60.0f);
	ExperienceByRank.Add(EBeyondEnemyRank::MiniBoss, 300.0f);
	ExperienceByRank.Add(EBeyondEnemyRank::Boss, 1500.0f);

	// EXP to the next level, damage dealt bonus, damage taken reduction
	const float Levels[][3] = {
		{ 40.0f, 0.0f, 0.0f }, { 80.0f, 0.05f, 0.03f }, { 120.0f, 0.12f, 0.08f }, { 170.0f, 0.15f, 0.10f }, { 230.0f, 0.18f, 0.12f },
		{ 300.0f, 0.21f, 0.14f }, { 380.0f, 0.24f, 0.16f }, { 470.0f, 0.27f, 0.18f }, { 570.0f, 0.30f, 0.20f }, { 0.0f, 0.35f, 0.22f },
	};
	for (const float (&Row)[3] : Levels)
	{
		FBeyondBondLevel& Level = BondLevels.AddDefaulted_GetRef();
		Level.ExperienceToNext = Row[0];
		Level.DamageDealtBonus = Row[1];
		Level.DamageTakenReduction = Row[2];
	}
}

const FBeyondBondLevel& UBeyondProgressionSettings::GetBondLevel(int32 Level) const
{
	static const FBeyondBondLevel None;
	if (BondLevels.IsEmpty())
	{
		return None;
	}
	return BondLevels[FMath::Clamp(Level - 1, 0, BondLevels.Num() - 1)];
}

float UBeyondProgressionSettings::GetExperienceToNextLevel(int32 Level)
{
	const UBeyondProgressionSettings* Settings = GetDefault<UBeyondProgressionSettings>();
	if (Level >= Settings->MaxLevel)
	{
		return 0.0f;
	}
	return FMath::RoundToFloat(Settings->BaseExperience * FMath::Pow(static_cast<float>(FMath::Max(Level, 1)), Settings->ExperienceExponent));
}

float UBeyondProgressionSettings::GetExperienceReward(EBeyondEnemyRank Rank, int32 EnemyLevel)
{
	const UBeyondProgressionSettings* Settings = GetDefault<UBeyondProgressionSettings>();
	const float* Base = Settings->ExperienceByRank.Find(Rank);
	const float LevelScale = 1.0f + Settings->ExperiencePerEnemyLevel * FMath::Max(EnemyLevel - 1, 0);
	return FMath::RoundToFloat((Base ? *Base : 0.0f) * LevelScale);
}

float UBeyondProgressionSettings::GetTotalExperience(int32 Level, float Experience)
{
	float Total = Experience;
	for (int32 Step = 1; Step < Level; ++Step)
	{
		Total += GetExperienceToNextLevel(Step);
	}
	return Total;
}
