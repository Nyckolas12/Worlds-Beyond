// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Items/BeyondItemTypes.h"
#include "Progression/BeyondProgressionSettings.h"
#include "BeyondLootSettings.generated.h"

class UFXSystemAsset;

/** What one enemy rank drops */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondLootRule
{
	GENERATED_BODY()

	// Chance that the enemy drops anything (guaranteed loot on the character drops regardless)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "1"))
	float DropChance = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinItems = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxItems = 1;

	// Relative weights of Common, Uncommon, Rare, Epic, Legendary
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", EditFixedSize)
	TArray<float> TierWeights = { 60.0f, 30.0f, 9.0f, 1.0f, 0.0f };

	// The first item is a set piece / the next one a weapon
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bGuaranteeSetPiece = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bGuaranteeWeapon = false;
};

/**
 * Items and loot (Project Settings -> Game -> Worlds Beyond Loot): the item database, how tier and level scale stats,
 * what each enemy rank drops, the drops' glow per tier and the starter kit. Saved in Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond Loot"))
class WORLDBEYOND_API UBeyondLootSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBeyondLootSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Every item that can drop (made by migrate_pass9.py)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Items")
	TSoftObjectPtr<UBeyondItemDatabase> ItemDatabase;

	// Stat multiplier per tier: Common, Uncommon, Rare, Epic, Legendary
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Items", EditFixedSize)
	TArray<float> TierStatScale = { 1.0f, 1.2f, 1.45f, 1.75f, 2.1f };

	// Extra stats per item level above 1 (0.08 = +8 %)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0"))
	float StatsPerItemLevel = 0.08f;

	// Set pieces never drop below this tier
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Items")
	EBeyondItemTier MinSetTier = EBeyondItemTier::Rare;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TMap<EBeyondEnemyRank, FBeyondLootRule> Rules;

	// Glow on a dropped item per tier (Common, Uncommon, Rare, Epic, Legendary)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loot|Drops", EditFixedSize)
	TArray<TSoftObjectPtr<UFXSystemAsset>> DropGlow;

	// How close a demigod must be to pick a drop up with F
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Loot|Drops", meta = (ClampMin = "50"))
	float PickupRange = 250.0f;

	// Given (and equipped where they fit) the first time the party forms without a save
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Starter Kit")
	TArray<TSoftObjectPtr<UBeyondItemDefinition>> StarterItems;

	const FBeyondLootRule* FindRule(EBeyondEnemyRank Rank) const { return Rules.Find(Rank); }
};
