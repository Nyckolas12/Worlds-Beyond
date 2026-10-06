// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	// Base stats x tier x level, plus bonus stats; one entry per stat
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
};
