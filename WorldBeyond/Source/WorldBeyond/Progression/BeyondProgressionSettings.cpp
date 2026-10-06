// Fill out your copyright notice in the Description page of Project Settings.

#include "Progression/BeyondProgressionSettings.h"

UBeyondProgressionSettings::UBeyondProgressionSettings()
{
	ExperienceByRank.Add(EBeyondEnemyRank::Regular, 25.0f);
	ExperienceByRank.Add(EBeyondEnemyRank::Elite, 60.0f);
	ExperienceByRank.Add(EBeyondEnemyRank::MiniBoss, 300.0f);
	ExperienceByRank.Add(EBeyondEnemyRank::Boss, 1500.0f);
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
