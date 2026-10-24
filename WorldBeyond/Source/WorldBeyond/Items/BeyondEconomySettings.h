// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Items/BeyondCurrency.h"
#include "Items/BeyondItemTypes.h"
#include "Progression/BeyondProgressionSettings.h"
#include "BeyondEconomySettings.generated.h"

/**
 * The party's money (Plan 6; Project Settings -> Game -> Worlds Beyond Economy): the starting purse, what kills and
 * discovery pay, what gear upgrades (+1 .. +15) cost and what salvaging gives back. Saved in Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond Economy"))
class WORLDBEYOND_API UBeyondEconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBeyondEconomySettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// A new party's purse (saves from before the wallet get it too)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Wallet")
	FBeyondCurrencyBundle StartingPurse;

	// What a kill pays the party, per enemy rank (training dummies and other no-loot characters pay nothing)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sources")
	TMap<EBeyondEnemyRank, FBeyondCurrencyBundle> KillRewards;

	// Kill gold per enemy level above 1 (0.12 = +12 %)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sources", meta = (ClampMin = "0"))
	float KillGoldPerLevel = 0.12f;

	// The first time the party walks into a region or village
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sources")
	FBeyondCurrencyBundle RegionDiscoveryReward;

	// The first time it finds a place (shrine, ruin, camp)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sources")
	FBeyondCurrencyBundle PlaceDiscoveryReward;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sources")
	FBeyondCurrencyBundle WaystoneAttuneReward;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "1", ClampMax = "30"))
	int32 MaxUpgradeLevel = 15;

	// Item stats per upgrade level (0.06 = +6 %)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "0"))
	float UpgradeStatsPerLevel = 0.06f;

	// Gold for +1 per tier (Common, Uncommon, Rare, Epic, Legendary); +N costs this x N ^ Upgrade Gold Exponent
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", EditFixedSize)
	TArray<int32> UpgradeGold = { 100, 150, 240, 380, 600 };

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "1"))
	float UpgradeGoldExponent = 1.5f;

	// From this upgrade on crystals are needed too: per tier x (levels from it, counting it)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "1"))
	int32 CrystalsFromUpgrade = 5;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", EditFixedSize)
	TArray<int32> UpgradeCrystals = { 1, 1, 2, 3, 5 };

	// From this upgrade on echo shards too
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "1"))
	int32 EchoShardsFromUpgrade = 10;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Upgrades", EditFixedSize)
	TArray<int32> UpgradeEchoShards = { 1, 1, 1, 2, 3 };

	// What salvaging an item gives per tier (the gold grows with item level like its stats)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Salvage", EditFixedSize)
	TArray<FBeyondCurrencyBundle> SalvageYield;

	// Share of what its upgrades cost that salvaging an item gives back
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Salvage", meta = (ClampMin = "0", ClampMax = "1"))
	float SalvageUpgradeRefund = 0.5f;

	// What buying upgrade Level (1 = +1) costs for an item of Tier
	FBeyondCurrencyBundle GetUpgradeStepCost(EBeyondItemTier Tier, int32 Level) const;

	// Everything +1 .. +Level cost together
	FBeyondCurrencyBundle GetUpgradeTotalCost(EBeyondItemTier Tier, int32 Level) const;

	FBeyondCurrencyBundle GetKillReward(EBeyondEnemyRank Rank, int32 EnemyLevel) const;
};
