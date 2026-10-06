// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/SoftObjectPath.h"
#include "BeyondSaveGame.generated.h"

/** One demigod's progress */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondMemberProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	int32 Level = 1;

	// EXP into the current level
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	float Experience = 0.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	int32 SkillPoints = 0;

	// Skill tree: node id -> rank
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Progress")
	TMap<FName, int32> SkillRanks;
};

/**
 * The party's progress (one slot, "BeyondProgress"). Written by UBeyondPartyComponent on level-ups, skill tree
 * changes, checkpoints and when play ends, read when the party is set up. The inventory (Plan 2) joins it later.
 */
UCLASS()
class WORLDBEYOND_API UBeyondSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

	// 1: levels (Plan 1A); 2: skill trees, Bond Points, duo loadout (Plan 1B)
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Version = 2;

	// Keyed by the demigod's class name (BP_Angel_C, BP_Ji-Woong_C)
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TMap<FName, FBeyondMemberProgress> Members;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 BondPoints = 0;

	// Highest party level Bond Points were already given for
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 BondPointsLevel = 0;

	// Duo tree: node id -> rank
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TMap<FName, int32> DuoRanks;

	// The duo power on G
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FSoftClassPath DuoLoadout;
};
