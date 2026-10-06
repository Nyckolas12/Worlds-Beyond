// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
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
};

/**
 * The party's progress (one slot, "BeyondProgress"). Written by UBeyondPartyComponent on level-ups, checkpoints and
 * when play ends, read when the party is set up. Skill tree unlocks (Plan 1B) and the inventory (Plan 2) join it later.
 */
UCLASS()
class WORLDBEYOND_API UBeyondSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SlotName;
	static constexpr int32 UserIndex = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Version = 1;

	// Keyed by the demigod's class name (BP_Angel_C, BP_Ji-Woong_C)
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TMap<FName, FBeyondMemberProgress> Members;
};
