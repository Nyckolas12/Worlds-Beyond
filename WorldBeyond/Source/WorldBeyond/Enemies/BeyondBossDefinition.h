// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "BeyondBossDefinition.generated.h"

class UAnimMontage;
class UBeyondAbilitySet;
class USoundBase;

UENUM(BlueprintType)
enum class EBeyondTwistType : uint8
{
	// Area hazards (lava rings, falling meteors, poison pools), once or on a repeat
	Hazards,
	// Calls roster enemies in (and again on a repeat, up to Max Alive)
	Summon,
	// Hits harder, moves faster, glows
	Enrage,
	// Weak copies of the boss join the fight
	ShadowClones,
	// The arena darkens (needs an ABeyondBossArena)
	Darkness
};

UENUM(BlueprintType)
enum class EBeyondHazardPlacement : uint8
{
	// On the arena's centre (rings, a lava floor)
	ArenaCentre,
	// Spread on a circle Spread away from the centre
	AroundArena,
	// Under each demigod, scattered by Spread
	AtTargets
};

/** Something that changes the fight when a phase starts */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondBossTwist
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twist")
	EBeyondTwistType Type = EBeyondTwistType::Hazards;

	// Shown as a banner when it happens ("The ground burns!")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twist")
	FText Announcement;

	// Hazards and summons happen again every this many seconds until the boss dies (0: once)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twist", meta = (ClampMin = "0"))
	float RepeatInterval = 0.0f;

	//~ Hazards

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazards", meta = (EditCondition = "Type == EBeyondTwistType::Hazards", EditConditionHides))
	FBeyondStrikeSettings Hazard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazards", meta = (ClampMin = "1", EditCondition = "Type == EBeyondTwistType::Hazards", EditConditionHides))
	int32 HazardCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazards", meta = (EditCondition = "Type == EBeyondTwistType::Hazards", EditConditionHides))
	EBeyondHazardPlacement Placement = EBeyondHazardPlacement::ArenaCentre;

	// Circle radius (Around Arena) or scatter (At Targets)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazards", meta = (ClampMin = "0", EditCondition = "Type == EBeyondTwistType::Hazards", EditConditionHides))
	float Spread = 0.0f;

	//~ Summon

	// Roster ids, picked at random per summon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon", meta = (EditCondition = "Type == EBeyondTwistType::Summon", EditConditionHides))
	TArray<FName> SummonIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "1", EditCondition = "Type == EBeyondTwistType::Summon", EditConditionHides))
	int32 SummonCount = 3;

	// No more of this boss's summons alive at once
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "1", EditCondition = "Type == EBeyondTwistType::Summon", EditConditionHides))
	int32 MaxAlive = 4;

	// Damage the boss takes x this while any of its summons lives (0.5: half damage while its pack protects it)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Type == EBeyondTwistType::Summon", EditConditionHides))
	float DamageTakenWhileSummonsLive = 1.0f;

	//~ Enrage

	// Added to Strength and Arcana (+1 % damage per point)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage", meta = (ClampMin = "0", EditCondition = "Type == EBeyondTwistType::Enrage", EditConditionHides))
	float EnrageDamageBonus = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage", meta = (ClampMin = "1", EditCondition = "Type == EBeyondTwistType::Enrage", EditConditionHides))
	float EnrageSpeedMultiplier = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage", meta = (EditCondition = "Type == EBeyondTwistType::Enrage", EditConditionHides))
	FLinearColor EnrageTint = FLinearColor(1.0f, 0.15f, 0.05f, 0.75f);

	// Looping on the boss while enraged (Max Lifetime 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage", meta = (EditCondition = "Type == EBeyondTwistType::Enrage", EditConditionHides))
	FBeyondFX EnrageFX;

	//~ Shadow clones

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clones", meta = (ClampMin = "1", EditCondition = "Type == EBeyondTwistType::ShadowClones", EditConditionHides))
	int32 CloneCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clones", meta = (ClampMin = "0.01", ClampMax = "1", EditCondition = "Type == EBeyondTwistType::ShadowClones", EditConditionHides))
	float CloneHealthFraction = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clones", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Type == EBeyondTwistType::ShadowClones", EditConditionHides))
	float CloneDamageFraction = 0.3f;

	//~ Darkness

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Darkness", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Type == EBeyondTwistType::Darkness", EditConditionHides))
	float Darkness = 0.65f;
};

/** One phase of a boss fight */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondBossPhase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FText Name;

	// The phase starts when health falls to this share of max health (the first phase: 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float HealthThreshold = 1.0f;

	// Played as the phase starts (roar, rise, enrage)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	TObjectPtr<UAnimMontage> TransitionMontage;

	// Seconds the boss can't be hurt or act while it changes phase (0: no transition)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0"))
	float TransitionDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FBeyondFX TransitionFX;

	// Extra abilities for this phase (abilities can also require the phase's Boss.Phase.N tag)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	TObjectPtr<UBeyondAbilitySet> Abilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (TitleProperty = "Type"))
	TArray<FBeyondBossTwist> Twists;
};

/**
 * A mini-boss or main boss (Plan 3B): an enemy definition with a title, health-threshold phases and twists, run by
 * ABeyondBossCharacter. Bosses use a Blueprint Character Class (the Paragon hero, reparented) for their mesh and anim
 * Blueprint, so Mesh can stay empty.
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondBossDefinition : public UBeyondEnemyDefinition
{
	GENERATED_BODY()

public:
	// Saved when a story boss falls (an arena never brings it back); also the console id
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FName BossId;

	// Under the name on the boss bar ("the Molten Colossus")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText Title;

	// Stays dead once beaten (main story bosses); mini-bosses can come back
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	bool bStoryBoss = true;

	// The first phase starts at 1; then in order of falling Health Threshold
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (TitleProperty = "Name"))
	TArray<FBeyondBossPhase> Phases;

	// Boss bar shows within this distance
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0"))
	float BarShowRadius = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USoundBase> Music;
};
