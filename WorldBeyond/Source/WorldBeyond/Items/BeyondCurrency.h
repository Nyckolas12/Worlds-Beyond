// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BeyondCurrency.generated.h"

/** The party's money (Plan 6): kept in the bag's wallet, shown on the menus */
UENUM(BlueprintType)
enum class EBeyondCurrency : uint8
{
	// Kills, salvage, quests; pays for upgrades
	Gold,
	// Elites, bosses, discovery, chapters; upgrades from +5
	Crystals,
	// Places, waystones, salvaging rare gear; upgrades from +10
	EchoShards,
	// Main chapters and story bosses
	ReverieFragments
};

/** Some of each currency: a price, a reward or what salvaging gives back */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondCurrencyBundle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Currency", meta = (ClampMin = "0"))
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Currency", meta = (ClampMin = "0"))
	int32 Crystals = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Currency", meta = (ClampMin = "0"))
	int32 EchoShards = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Currency", meta = (ClampMin = "0"))
	int32 ReverieFragments = 0;

	static constexpr EBeyondCurrency All[] = { EBeyondCurrency::Gold, EBeyondCurrency::Crystals, EBeyondCurrency::EchoShards, EBeyondCurrency::ReverieFragments };

	FBeyondCurrencyBundle() = default;
	explicit FBeyondCurrencyBundle(int32 InGold, int32 InCrystals = 0, int32 InEchoShards = 0, int32 InReverieFragments = 0)
		: Gold(InGold), Crystals(InCrystals), EchoShards(InEchoShards), ReverieFragments(InReverieFragments)
	{
	}

	int32 Get(EBeyondCurrency Currency) const
	{
		switch (Currency)
		{
		case EBeyondCurrency::Crystals: return Crystals;
		case EBeyondCurrency::EchoShards: return EchoShards;
		case EBeyondCurrency::ReverieFragments: return ReverieFragments;
		default: return Gold;
		}
	}

	void Add(EBeyondCurrency Currency, int32 Amount)
	{
		switch (Currency)
		{
		case EBeyondCurrency::Crystals: Crystals += Amount; break;
		case EBeyondCurrency::EchoShards: EchoShards += Amount; break;
		case EBeyondCurrency::ReverieFragments: ReverieFragments += Amount; break;
		default: Gold += Amount; break;
		}
	}

	bool IsEmpty() const { return Gold <= 0 && Crystals <= 0 && EchoShards <= 0 && ReverieFragments <= 0; }

	FBeyondCurrencyBundle& operator+=(const FBeyondCurrencyBundle& Other)
	{
		for (const EBeyondCurrency Currency : All)
		{
			Add(Currency, Other.Get(Currency));
		}
		return *this;
	}

	bool operator==(const FBeyondCurrencyBundle& Other) const
	{
		return Gold == Other.Gold && Crystals == Other.Crystals && EchoShards == Other.EchoShards && ReverieFragments == Other.ReverieFragments;
	}
};
