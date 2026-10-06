// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BeyondProgressionSettings.generated.h"

/** How strong an enemy is: decides how much EXP it is worth */
UENUM(BlueprintType)
enum class EBeyondEnemyRank : uint8
{
	// Open-world enemies
	Regular,
	// Tougher variants (casters, captains, affixed enemies)
	Elite,
	MiniBoss,
	// Main story bosses
	Boss
};

/** What a character gains per level above 1 (applied through UBeyondGE_LevelStats) */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondStatGrowth
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
	float MaxHealth = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
	float MaxStamina = 4.0f;

	// Melee damage: +1 % per point
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
	float Strength = 2.0f;

	// Ability / projectile damage: +1 % per point
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
	float Arcana = 2.0f;

	// Damage taken x 100 / (100 + Defense)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth", meta = (ClampMin = "0"))
	float Defense = 1.0f;
};

/**
 * Leveling rules (Project Settings -> Game -> Worlds Beyond Progression): how much EXP each level needs, what enemies
 * are worth and how many skill points a level gives. Saved in Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond Progression"))
class WORLDBEYOND_API UBeyondProgressionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBeyondProgressionSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// EXP from level L to L + 1 = Base Experience x L ^ Experience Exponent
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (ClampMin = "1"))
	float BaseExperience = 100.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (ClampMin = "1", ClampMax = "4"))
	float ExperienceExponent = 1.5f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (ClampMin = "1"))
	int32 MaxLevel = 50;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (ClampMin = "0"))
	int32 SkillPointsPerLevel = 1;

	// EXP an enemy of each rank is worth at level 1 (a character's Experience Reward overrides it)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	TMap<EBeyondEnemyRank, float> ExperienceByRank;

	// Extra EXP per enemy level above 1 (0.1 = +10 % per level)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
	float ExperiencePerEnemyLevel = 0.1f;

	// EXP needed to go from Level to Level + 1 (0 at the max level)
	UFUNCTION(BlueprintPure, Category = "Progression")
	static float GetExperienceToNextLevel(int32 Level);

	// EXP a rank is worth at a level
	UFUNCTION(BlueprintPure, Category = "Progression")
	static float GetExperienceReward(EBeyondEnemyRank Rank, int32 EnemyLevel);

	// All the EXP it takes to reach Level from level 1, plus Experience into it
	UFUNCTION(BlueprintPure, Category = "Progression")
	static float GetTotalExperience(int32 Level, float Experience);
};
