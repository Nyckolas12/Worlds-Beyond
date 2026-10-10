// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/BeyondLootSettings.h"

namespace
{
	FBeyondLootRule MakeLootRule(float Chance, int32 MinCount, int32 MaxCount, std::initializer_list<float> Weights, bool bSetPiece, bool bWeapon)
	{
		FBeyondLootRule Rule;
		Rule.DropChance = Chance;
		Rule.MinItems = MinCount;
		Rule.MaxItems = MaxCount;
		Rule.TierWeights = TArray<float>(Weights);
		Rule.bGuaranteeSetPiece = bSetPiece;
		Rule.bGuaranteeWeapon = bWeapon;
		return Rule;
	}

	TSoftObjectPtr<UBeyondItemDefinition> MakeStarterItemRef(const TCHAR* Path)
	{
		return TSoftObjectPtr<UBeyondItemDefinition>(FSoftObjectPath(Path));
	}
}

UBeyondLootSettings::UBeyondLootSettings()
{
	ItemDatabase = TSoftObjectPtr<UBeyondItemDatabase>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Items/DA_ItemDatabase.DA_ItemDatabase")));

	Rules.Add(EBeyondEnemyRank::Regular, MakeLootRule(0.12f, 1, 1, { 60.0f, 30.0f, 9.0f, 1.0f, 0.0f }, false, false));
	Rules.Add(EBeyondEnemyRank::Elite, MakeLootRule(0.35f, 1, 1, { 30.0f, 40.0f, 24.0f, 5.0f, 1.0f }, false, false));
	Rules.Add(EBeyondEnemyRank::MiniBoss, MakeLootRule(1.0f, 2, 2, { 0.0f, 30.0f, 45.0f, 20.0f, 5.0f }, true, false));
	Rules.Add(EBeyondEnemyRank::Boss, MakeLootRule(1.0f, 3, 3, { 0.0f, 0.0f, 45.0f, 40.0f, 15.0f }, true, true));

	// The Paragon Minions pack's ground pickups, one colour per tier
	const TCHAR* Glows[] = {
		TEXT("/Game/ParagonMinions/FX/Particles/PlayerBuffs/P_CarriedBuff_GroundPickup.P_CarriedBuff_GroundPickup"),
		TEXT("/Game/ParagonMinions/FX/Particles/PlayerBuffs/P_CarriedBuff_GroundPickup_Green.P_CarriedBuff_GroundPickup_Green"),
		TEXT("/Game/ParagonMinions/FX/Particles/PlayerBuffs/P_CarriedBuff_GroundPickup_Mid_Blue.P_CarriedBuff_GroundPickup_Mid_Blue"),
		TEXT("/Game/ParagonMinions/FX/Particles/PlayerBuffs/P_CarriedBuff_GroundPickup_Mid_Purple.P_CarriedBuff_GroundPickup_Mid_Purple"),
		TEXT("/Game/ParagonMinions/FX/Particles/PlayerBuffs/P_CarriedBuff_GroundPickup_Gold.P_CarriedBuff_GroundPickup_Gold"),
	};
	for (const TCHAR* Glow : Glows)
	{
		DropGlow.Add(TSoftObjectPtr<UFXSystemAsset>(FSoftObjectPath(Glow)));
	}

	StarterItems.Add(MakeStarterItemRef(TEXT("/Game/WorldsBeyond/Items/Weapons/DA_Item_TravelersBlade.DA_Item_TravelersBlade")));
	StarterItems.Add(MakeStarterItemRef(TEXT("/Game/WorldsBeyond/Items/Weapons/DA_Item_AshwoodStaff.DA_Item_AshwoodStaff")));
	StarterItems.Add(MakeStarterItemRef(TEXT("/Game/WorldsBeyond/Items/Armor/DA_Item_WanderersChest.DA_Item_WanderersChest")));
	StarterItems.Add(MakeStarterItemRef(TEXT("/Game/WorldsBeyond/Items/Armor/DA_Item_WanderersBoots.DA_Item_WanderersBoots")));
}
