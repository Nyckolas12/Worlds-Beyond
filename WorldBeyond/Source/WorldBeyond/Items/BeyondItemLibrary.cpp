// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondItemLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "Items/BeyondLootSettings.h"

#define LOCTEXT_NAMESPACE "BeyondItems"

namespace
{
	// Size of a rolled bonus stat at item level 1
	float BonusStatBase(EBeyondSkillStat Stat)
	{
		switch (Stat)
		{
		case EBeyondSkillStat::MaxHealth: return 10.0f;
		case EBeyondSkillStat::MaxStamina: return 6.0f;
		default: return 3.0f;
		}
	}

	float ItemLevelScale(int32 ItemLevel)
	{
		return 1.0f + GetDefault<UBeyondLootSettings>()->StatsPerItemLevel * FMath::Max(ItemLevel - 1, 0);
	}

	EBeyondItemTier RollItemTier(const TArray<float>& Weights, FRandomStream& Stream)
	{
		float Total = 0.0f;
		for (const float Weight : Weights)
		{
			Total += FMath::Max(Weight, 0.0f);
		}
		if (Total <= 0.0f)
		{
			return EBeyondItemTier::Common;
		}
		float Pick = Stream.FRandRange(0.0f, Total);
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			Pick -= FMath::Max(Weights[Index], 0.0f);
			if (Pick <= 0.0f)
			{
				return static_cast<EBeyondItemTier>(FMath::Clamp(Index, 0, static_cast<int32>(EBeyondItemTier::Legendary)));
			}
		}
		return EBeyondItemTier::Common;
	}

	const UBeyondItemDefinition* PickItemDefinition(const UBeyondItemDatabase& Database, TFunctionRef<bool(const UBeyondItemDefinition&)> Filter, FRandomStream& Stream)
	{
		float Total = 0.0f;
		for (const UBeyondItemDefinition* Item : Database.Items)
		{
			if (Item && Item->DropWeight > 0.0f && Filter(*Item))
			{
				Total += Item->DropWeight;
			}
		}
		if (Total <= 0.0f)
		{
			return nullptr;
		}
		float Pick = Stream.FRandRange(0.0f, Total);
		for (const UBeyondItemDefinition* Item : Database.Items)
		{
			if (Item && Item->DropWeight > 0.0f && Filter(*Item))
			{
				Pick -= Item->DropWeight;
				if (Pick <= 0.0f)
				{
					return Item;
				}
			}
		}
		return nullptr;
	}

	EBeyondItemTier ClampItemTier(const UBeyondItemDefinition& Definition, EBeyondItemTier Tier)
	{
		int32 Value = FMath::Clamp(static_cast<int32>(Tier), static_cast<int32>(Definition.MinTier), static_cast<int32>(Definition.MaxTier));
		if (Definition.Set)
		{
			Value = FMath::Max(Value, static_cast<int32>(GetDefault<UBeyondLootSettings>()->MinSetTier));
		}
		return static_cast<EBeyondItemTier>(Value);
	}
}

FLinearColor UBeyondItemLibrary::GetTierColor(EBeyondItemTier Tier)
{
	switch (Tier)
	{
	case EBeyondItemTier::Uncommon: return FLinearColor(0.25f, 0.9f, 0.3f);
	case EBeyondItemTier::Rare: return FLinearColor(0.2f, 0.5f, 1.0f);
	case EBeyondItemTier::Epic: return FLinearColor(0.7f, 0.3f, 1.0f);
	case EBeyondItemTier::Legendary: return FLinearColor(1.0f, 0.72f, 0.12f);
	default: return FLinearColor(0.85f, 0.85f, 0.88f);
	}
}

FText UBeyondItemLibrary::GetTierName(EBeyondItemTier Tier)
{
	switch (Tier)
	{
	case EBeyondItemTier::Uncommon: return LOCTEXT("Uncommon", "Uncommon");
	case EBeyondItemTier::Rare: return LOCTEXT("Rare", "Rare");
	case EBeyondItemTier::Epic: return LOCTEXT("Epic", "Epic");
	case EBeyondItemTier::Legendary: return LOCTEXT("Legendary", "Legendary");
	default: return LOCTEXT("Common", "Common");
	}
}

FText UBeyondItemLibrary::GetSlotName(EBeyondItemSlot Slot)
{
	switch (Slot)
	{
	case EBeyondItemSlot::Helm: return LOCTEXT("Helm", "Helm");
	case EBeyondItemSlot::Chest: return LOCTEXT("Chest", "Chest");
	case EBeyondItemSlot::Gauntlets: return LOCTEXT("Gauntlets", "Gauntlets");
	case EBeyondItemSlot::Boots: return LOCTEXT("Boots", "Boots");
	default: return LOCTEXT("Weapon", "Weapon");
	}
}

FText UBeyondItemLibrary::GetItemName(const FBeyondItemInstance& Item)
{
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	return Definition ? Definition->DisplayName : LOCTEXT("UnknownItem", "Unknown item");
}

FText UBeyondItemLibrary::FormatStat(const FBeyondItemStat& Stat)
{
	return FText::Format(LOCTEXT("StatLine", "+{0} {1}"), FText::AsNumber(FMath::RoundToInt(Stat.Value)), UBeyondSkillTreeAsset::GetStatName(Stat.Stat));
}

TArray<FBeyondItemStat> UBeyondItemLibrary::GetItemStats(const FBeyondItemInstance& Item)
{
	TArray<FBeyondItemStat> Result;
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	if (!Definition)
	{
		return Result;
	}

	const UBeyondLootSettings* Settings = GetDefault<UBeyondLootSettings>();
	const int32 TierIndex = static_cast<int32>(Item.Tier);
	const float TierScale = Settings->TierStatScale.IsValidIndex(TierIndex) ? Settings->TierStatScale[TierIndex] : 1.0f;
	const float Scale = TierScale * ItemLevelScale(Item.ItemLevel);

	auto AddStat = [&Result](EBeyondSkillStat Stat, float Value)
	{
		if (FBeyondItemStat* Existing = Result.FindByPredicate([Stat](const FBeyondItemStat& Entry) { return Entry.Stat == Stat; }))
		{
			Existing->Value += Value;
		}
		else
		{
			FBeyondItemStat& Added = Result.AddDefaulted_GetRef();
			Added.Stat = Stat;
			Added.Value = Value;
		}
	};
	for (const FBeyondItemStat& Stat : Definition->BaseStats)
	{
		AddStat(Stat.Stat, FMath::RoundToFloat(Stat.Value * Scale));
	}
	for (const FBeyondItemStat& Stat : Item.BonusStats)
	{
		AddStat(Stat.Stat, Stat.Value);
	}
	return Result;
}

float UBeyondItemLibrary::GetItemStat(const FBeyondItemInstance& Item, EBeyondSkillStat Stat)
{
	for (const FBeyondItemStat& Entry : GetItemStats(Item))
	{
		if (Entry.Stat == Stat)
		{
			return Entry.Value;
		}
	}
	return 0.0f;
}

bool UBeyondItemLibrary::CanCharacterUse(const FBeyondItemInstance& Item, const ABeyondCharacterBase* Character, FText& OutReason)
{
	const UBeyondItemDefinition* Definition = Item.GetDefinition();
	if (!Definition || !Character)
	{
		OutReason = LOCTEXT("NoItem", "Unknown item");
		return false;
	}
	if (Definition->Slot == EBeyondItemSlot::Weapon && Definition->WeaponTag.IsValid())
	{
		const FGameplayTag& CharacterWeapon = Character->GetDefaultWeaponTag();
		if (!CharacterWeapon.IsValid() || !CharacterWeapon.MatchesTag(Definition->WeaponTag))
		{
			OutReason = FText::Format(LOCTEXT("WrongWeapon", "{0} can't wield this"), Character->GetCharacterDisplayName());
			return false;
		}
	}
	OutReason = FText::GetEmpty();
	return true;
}

FBeyondItemInstance UBeyondItemLibrary::MakeItem(const UBeyondItemDefinition* Definition, EBeyondItemTier Tier, int32 ItemLevel)
{
	FRandomStream Stream(FMath::Rand());
	return MakeItemFromStream(Definition, Tier, ItemLevel, Stream);
}

FBeyondItemInstance UBeyondItemLibrary::MakeItemFromStream(const UBeyondItemDefinition* Definition, EBeyondItemTier Tier, int32 ItemLevel, FRandomStream& Stream)
{
	FBeyondItemInstance Item;
	if (!Definition)
	{
		return Item;
	}
	Item.Id = FGuid::NewGuid();
	Item.Definition = const_cast<UBeyondItemDefinition*>(Definition);
	Item.Tier = ClampItemTier(*Definition, Tier);
	Item.ItemLevel = FMath::Max(ItemLevel, 1);

	// Epic items roll one bonus stat, Legendary two
	const int32 Bonuses = Item.Tier == EBeyondItemTier::Legendary ? 2 : Item.Tier == EBeyondItemTier::Epic ? 1 : 0;
	static const EBeyondSkillStat Pool[] = { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength,
		EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense };
	for (int32 Index = 0; Index < Bonuses; ++Index)
	{
		FBeyondItemStat& Bonus = Item.BonusStats.AddDefaulted_GetRef();
		Bonus.Stat = Pool[Stream.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Pool)) - 1)];
		Bonus.Value = FMath::RoundToFloat(BonusStatBase(Bonus.Stat) * ItemLevelScale(Item.ItemLevel) * Stream.FRandRange(0.8f, 1.2f));
	}
	return Item;
}

UBeyondItemDatabase* UBeyondItemLibrary::GetItemDatabase()
{
	return GetDefault<UBeyondLootSettings>()->ItemDatabase.LoadSynchronous();
}

TArray<FBeyondItemInstance> UBeyondItemLibrary::RollLootForEnemy(const ABeyondCharacterBase* Victim, int32 Seed)
{
	FRandomStream Stream(Seed);
	return RollLoot(Victim, Stream);
}

TArray<FBeyondItemInstance> UBeyondItemLibrary::RollLoot(const ABeyondCharacterBase* Victim, FRandomStream& Stream)
{
	TArray<FBeyondItemInstance> Drops;
	if (!Victim)
	{
		return Drops;
	}
	const UBeyondLootSettings* Settings = GetDefault<UBeyondLootSettings>();
	const FBeyondLootRule* Rule = Settings->FindRule(Victim->Rank);
	const int32 ItemLevel = Victim->GetCharacterLevel();
	const TArray<float> DefaultWeights = { 60.0f, 30.0f, 9.0f, 1.0f, 0.0f };
	const TArray<float>& Weights = Rule ? Rule->TierWeights : DefaultWeights;

	// This enemy's own drops (boss loot) always come
	for (const TSoftObjectPtr<UBeyondItemDefinition>& Guaranteed : Victim->GuaranteedLoot)
	{
		if (const UBeyondItemDefinition* Definition = Guaranteed.LoadSynchronous())
		{
			Drops.Add(MakeItemFromStream(Definition, RollItemTier(Weights, Stream), ItemLevel, Stream));
		}
	}

	const UBeyondItemDatabase* Database = GetItemDatabase();
	if (!Rule || !Database || Stream.FRand() > Rule->DropChance)
	{
		return Drops;
	}

	const int32 Count = Stream.RandRange(FMath::Max(Rule->MinItems, 0), FMath::Max(Rule->MaxItems, Rule->MinItems));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const UBeyondItemDefinition* Definition = nullptr;
		if (Index == 0 && Rule->bGuaranteeSetPiece)
		{
			Definition = PickItemDefinition(*Database, [](const UBeyondItemDefinition& Item) { return Item.Set != nullptr; }, Stream);
		}
		else if (Index == (Rule->bGuaranteeSetPiece ? 1 : 0) && Rule->bGuaranteeWeapon)
		{
			Definition = PickItemDefinition(*Database, [](const UBeyondItemDefinition& Item) { return Item.Slot == EBeyondItemSlot::Weapon; }, Stream);
		}
		if (!Definition)
		{
			Definition = PickItemDefinition(*Database, [](const UBeyondItemDefinition&) { return true; }, Stream);
		}
		if (Definition)
		{
			Drops.Add(MakeItemFromStream(Definition, RollItemTier(Weights, Stream), ItemLevel, Stream));
		}
	}
	return Drops;
}

#undef LOCTEXT_NAMESPACE
