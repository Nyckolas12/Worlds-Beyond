// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/BeyondCurrency.h"
#include "Items/BeyondItemTypes.h"
#include "BeyondQuestTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EBeyondQuestKind : uint8
{
	// The story's chapters
	Main,
	Side
};

/** What a quest objective waits for (Plan 6) */
UENUM(BlueprintType)
enum class EBeyondObjectiveType : uint8
{
	// A story flag (Target) is set. The world and the bosses answer too: Discovered.<region / village / place / waystone>,
	// Waystone.<id> (attuned), Boss.<id> (beaten), Talked.<NPC>.<row>, Quest.<id> (another quest done)
	Flag,
	// Defeat Count enemies after the quest starts (Target: their roster id, forest_wolf; none: any foe)
	KillEnemies,
	// Carry Count of an item, in the bag or worn (Target: its asset name, DA_Item_GildedSaber)
	CollectItem,
	// The party reaches level Count
	ReachLevel,
	// Count waystones attuned
	AttuneWaystones,
	// The duo's Bond level reaches Count
	ReachBondLevel,
	// Another quest (Target: its id) is complete
	CompleteQuest,
	// Talk to an NPC (Target: its NPC Id)
	TalkTo
};

UENUM(BlueprintType)
enum class EBeyondQuestState : uint8
{
	// Its requirements aren't met yet
	Locked,
	Active,
	Completed
};

USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EBeyondObjectiveType Type = EBeyondObjectiveType::Flag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName Target;

	// How many (kills, items, level, waystones, Bond level)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 Count = 1;

	// What the log shows ("Reach Mossbrook")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Text;
};

/** Given once, when the quest completes */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondQuestReward
{
	GENERATED_BODY()

	// EXP for each demigod
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
	float Experience = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	FBeyondCurrencyBundle Currencies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
	float BondExperience = 0.0f;

	// Made at the party's level and this tier
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<TSoftObjectPtr<UBeyondItemDefinition>> Items;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	EBeyondItemTier ItemTier = EBeyondItemTier::Rare;
};

USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondQuest
{
	GENERATED_BODY()

	// Saves refer to it: don't rename quests players may have started
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EBeyondQuestKind Kind = EBeyondQuestKind::Side;

	// Main story: its chapter (1, 2, ...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "0"))
	int32 Chapter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = "true"))
	FText Summary;

	// Where it happens ("Mossbrook"), shown under the title
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	// It starts once these quests are done, this flag is set (none: no flag) and the party has this level
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Start")
	TArray<FName> Requires;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Start")
	FName RequiredFlag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Start", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	// All of them complete the quest
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FBeyondQuestObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FBeyondQuestReward Reward;

	// Set when it completes (dialogue and other quests can check it); none: Quest.<Quest Id>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName CompletionFlag;

	FName GetCompletionFlag() const { return CompletionFlag.IsNone() ? FName(*(TEXT("Quest.") + QuestId.ToString())) : CompletionFlag; }
};

/** Every quest (made by migrate_pass22.py from quest_content.py; tune it in the editor) */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondQuestDatabase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The log lists them in this order (main chapters, then side quests)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests", meta = (TitleProperty = "QuestId"))
	TArray<FBeyondQuest> Quests;

	const FBeyondQuest* FindQuest(FName QuestId) const;

	// Duplicate or missing ids, unknown prerequisites, quests without objectives; empty when fine
	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<FString> ValidateQuests() const;
};

/** One quest's saved progress */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	// Kills and talks so far, per objective
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest")
	TArray<int32> Counts;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Quest")
	bool bCompleted = false;
};
