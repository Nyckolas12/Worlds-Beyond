// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/BeyondCurrency.h"
#include "Items/BeyondItemTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BeyondItemLibrary.generated.h"

class ABeyondCharacterBase;

/** Item helpers: final stats from tier and level, names and colours, who can use what, and rolling loot. */
UCLASS()
class WORLDBEYOND_API UBeyondItemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FLinearColor GetTierColor(EBeyondItemTier Tier);

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FText GetTierName(EBeyondItemTier Tier);

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FText GetSlotName(EBeyondItemSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FText GetItemName(const FBeyondItemInstance& Item);

	// "+12 Defense"
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FText FormatStat(const FBeyondItemStat& Stat);

	// Base stats x tier x level, plus bonus stats, all x the upgrade; one entry per stat
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static TArray<FBeyondItemStat> GetItemStats(const FBeyondItemInstance& Item);

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static float GetItemStat(const FBeyondItemInstance& Item, EBeyondSkillStat Stat);

	// Whether Character may wear / wield it (weapons need the character's weapon type); OutReason says why not
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static bool CanCharacterUse(const FBeyondItemInstance& Item, const ABeyondCharacterBase* Character, FText& OutReason);

	// A new item (random bonus stats for Epic / Legendary)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Items")
	static FBeyondItemInstance MakeItem(const UBeyondItemDefinition* Definition, EBeyondItemTier Tier, int32 ItemLevel);

	static FBeyondItemInstance MakeItemFromStream(const UBeyondItemDefinition* Definition, EBeyondItemTier Tier, int32 ItemLevel, FRandomStream& Stream);

	// What a defeated enemy drops (its rank's rule from the loot settings, plus its Guaranteed Loot)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Items")
	static TArray<FBeyondItemInstance> RollLootForEnemy(const ABeyondCharacterBase* Victim, int32 Seed);

	static TArray<FBeyondItemInstance> RollLoot(const ABeyondCharacterBase* Victim, FRandomStream& Stream);

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static UBeyondItemDatabase* GetItemDatabase();

	//~ Plan 6: upgrades, salvage, currencies

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static int32 GetMaxUpgradeLevel();

	// What the next upgrade costs; empty when it is at the maximum
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FBeyondCurrencyBundle GetUpgradeCost(const FBeyondItemInstance& Item);

	// The item one upgrade higher (stats, cost and limits aside)
	static FBeyondItemInstance WithUpgrade(const FBeyondItemInstance& Item, int32 Levels = 1);

	// What salvaging it gives: its tier's share (more for higher item levels) plus part of what its upgrades cost
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FBeyondCurrencyBundle GetSalvageYield(const FBeyondItemInstance& Item);

	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FText GetCurrencyName(EBeyondCurrency Currency);

	// "1,200 Gold, 3 Crystals" (empty: "nothing")
	UFUNCTION(BlueprintPure, Category = "Beyond|Items")
	static FText FormatCurrencies(const FBeyondCurrencyBundle& Bundle);
};
