// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondEconomySettings.h"

namespace
{
	int32 TierValue(const TArray<int32>& Values, EBeyondItemTier Tier)
	{
		const int32 Index = static_cast<int32>(Tier);
		return Values.IsValidIndex(Index) ? Values[Index] : (Values.IsEmpty() ? 0 : Values.Last());
	}
}

UBeyondEconomySettings::UBeyondEconomySettings()
{
	StartingPurse = FBeyondCurrencyBundle(500);

	KillRewards.Add(EBeyondEnemyRank::Regular, FBeyondCurrencyBundle(8));
	KillRewards.Add(EBeyondEnemyRank::Elite, FBeyondCurrencyBundle(30, 1));
	KillRewards.Add(EBeyondEnemyRank::MiniBoss, FBeyondCurrencyBundle(120, 4, 1));
	KillRewards.Add(EBeyondEnemyRank::Boss, FBeyondCurrencyBundle(400, 10, 3, 1));

	RegionDiscoveryReward = FBeyondCurrencyBundle(0, 2, 1);
	PlaceDiscoveryReward = FBeyondCurrencyBundle(25, 0, 1);
	WaystoneAttuneReward = FBeyondCurrencyBundle(0, 0, 2);

	SalvageYield = {
		FBeyondCurrencyBundle(15),
		FBeyondCurrencyBundle(30),
		FBeyondCurrencyBundle(60, 1),
		FBeyondCurrencyBundle(120, 2, 1),
		FBeyondCurrencyBundle(240, 4, 2),
	};
}

FBeyondCurrencyBundle UBeyondEconomySettings::GetUpgradeStepCost(EBeyondItemTier Tier, int32 Level) const
{
	FBeyondCurrencyBundle Cost;
	if (Level < 1 || Level > MaxUpgradeLevel)
	{
		return Cost;
	}
	Cost.Gold = FMath::RoundToInt(TierValue(UpgradeGold, Tier) * FMath::Pow(static_cast<float>(Level), UpgradeGoldExponent));
	if (Level >= CrystalsFromUpgrade)
	{
		Cost.Crystals = TierValue(UpgradeCrystals, Tier) * (Level - CrystalsFromUpgrade + 1);
	}
	if (Level >= EchoShardsFromUpgrade)
	{
		Cost.EchoShards = TierValue(UpgradeEchoShards, Tier) * (Level - EchoShardsFromUpgrade + 1);
	}
	return Cost;
}

FBeyondCurrencyBundle UBeyondEconomySettings::GetUpgradeTotalCost(EBeyondItemTier Tier, int32 Level) const
{
	FBeyondCurrencyBundle Total;
	for (int32 Step = 1; Step <= FMath::Min(Level, MaxUpgradeLevel); ++Step)
	{
		Total += GetUpgradeStepCost(Tier, Step);
	}
	return Total;
}

FBeyondCurrencyBundle UBeyondEconomySettings::GetKillReward(EBeyondEnemyRank Rank, int32 EnemyLevel) const
{
	FBeyondCurrencyBundle Reward = KillRewards.FindRef(Rank);
	Reward.Gold = FMath::RoundToInt(Reward.Gold * (1.0f + KillGoldPerLevel * FMath::Max(EnemyLevel - 1, 0)));
	return Reward;
}
