// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUIDemo.h"
#include "Characters/BeyondCharacterBase.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondSkillTreeComponent.h"
#include "Quests/BeyondQuestComponent.h"
#include "WorldBeyond.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs DemoStateCommand(
		TEXT("Beyond.UI.DemoState"),
		TEXT("A mid-game party for menu screenshots (overwrites the party's progress: set Beyond.SaveProgress 0 first)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!BeyondUIDemo::ApplyDemoState(World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr))
			{
				UE_LOG(LogBeyond, Warning, TEXT("Beyond.UI.DemoState needs a game running with the party formed"));
			}
		}));

	struct FDemoItem
	{
		FString Name;
		EBeyondItemTier Tier;
		int32 Level;
		int32 Upgrade;
		bool bLocked;
		bool bJunk;
	};

	const UBeyondItemDefinition* FindDefinition(const FString& Name)
	{
		const UBeyondItemDatabase* Database = UBeyondItemLibrary::GetItemDatabase();
		for (const UBeyondItemDefinition* Definition : Database ? Database->Items : TArray<TObjectPtr<UBeyondItemDefinition>>())
		{
			if (Definition && Definition->GetName() == TEXT("DA_Item_") + Name)
			{
				return Definition;
			}
		}
		return nullptr;
	}

	FBeyondItemInstance MakeDemoItem(const FDemoItem& Spec)
	{
		FRandomStream Stream(GetTypeHash(Spec.Name) ^ Spec.Level);
		FBeyondItemInstance Item = UBeyondItemLibrary::MakeItemFromStream(FindDefinition(Spec.Name), Spec.Tier, Spec.Level, Stream);
		Item.UpgradeLevel = Spec.Upgrade;
		Item.bLocked = Spec.bLocked;
		Item.bJunk = Spec.bJunk;
		return Item;
	}
}

bool BeyondUIDemo::ApplyDemoState(ABeyondPlayerController* PC)
{
	UBeyondPartyComponent* Party = PC ? PC->PartyComponent.Get() : nullptr;
	UBeyondInventoryComponent* Bag = PC ? PC->InventoryComponent.Get() : nullptr;
	if (!Party || !Bag || !Party->IsPartyFormed())
	{
		return false;
	}

	//~ Levels and skills
	const TMap<FName, int32> AngelRanks = { { TEXT("Angel.Arcana1"), 3 }, { TEXT("Angel.Vitality1"), 2 }, { TEXT("Angel.Spikes"), 2 }, { TEXT("Angel.Bolt"), 1 },
		{ TEXT("Angel.KeenStatic"), 2 }, { TEXT("Angel.StormsEmbrace"), 2 }, { TEXT("Angel.Arcana2"), 1 }, { TEXT("Angel.Ward"), 1 } };
	const TMap<FName, int32> JiWoongRanks = { { TEXT("JiWoong.Strength1"), 3 }, { TEXT("JiWoong.Vitality1"), 2 }, { TEXT("JiWoong.Combo"), 2 },
		{ TEXT("JiWoong.Brand"), 2 }, { TEXT("JiWoong.Hex"), 1 }, { TEXT("JiWoong.RadiantZeal"), 1 }, { TEXT("JiWoong.Guard"), 1 } };
	for (ABeyondCharacterBase* Member : Party->GetMembers())
	{
		if (!IsValid(Member) || !Member->CanGainExperience())
		{
			continue;
		}
		const bool bAngel = Member->DuoRole == EBeyondDuoRole::Conduit;
		Member->RestoreProgress(24, 3400.0f, 3);
		if (UBeyondSkillTreeComponent* Tree = Member->GetSkillTreeComponent())
		{
			Tree->RestoreRanks(bAngel ? AngelRanks : JiWoongRanks);
		}

		//~ Gear: the demigod's armor set, a weapon, a ring and a charm, upgraded
		UBeyondEquipmentComponent* Equipment = Member->GetEquipmentComponent();
		if (!Equipment)
		{
			continue;
		}
		const FString Set = bAngel ? TEXT("Stormforged") : TEXT("Sunforged");
		const FDemoItem Worn[] = {
			{ Set + TEXT("Helm"), EBeyondItemTier::Epic, 22, 5, false, false },
			{ Set + TEXT("Chest"), EBeyondItemTier::Epic, 23, 7, true, false },
			{ Set + TEXT("Gauntlets"), EBeyondItemTier::Rare, 21, 4, false, false },
			{ Set + TEXT("Boots"), EBeyondItemTier::Epic, 22, 5, false, false },
			{ bAngel ? TEXT("StormcallersRod") : TEXT("Dawnbreaker"), EBeyondItemTier::Legendary, 24, 9, true, false },
			{ bAngel ? TEXT("BandOfStatic") : TEXT("SunstoneSignet"), EBeyondItemTier::Rare, 20, 3, false, false },
			{ bAngel ? TEXT("StormglassAmulet") : TEXT("EclipseTalisman"), EBeyondItemTier::Epic, 21, 4, false, false },
		};
		for (const FDemoItem& Spec : Worn)
		{
			const FBeyondItemInstance Item = MakeDemoItem(Spec);
			if (Item.IsValid())
			{
				Equipment->EquipItem(Item);
			}
		}
	}

	if (UBeyondSkillTreeComponent* DuoTree = PC->DuoSkillTree.Get())
	{
		DuoTree->RestoreRanks({ { TEXT("Duo.Kindred"), 2 }, { TEXT("Duo.Wrath"), 1 }, { TEXT("Duo.Eclipse"), 1 } });
	}
	Party->AddBondPoints(2);

	//~ The bag: what the starter kit left plus a mix of finds
	TArray<FBeyondItemInstance> BagItems;
	for (const FBeyondItemInstance& Item : Bag->GetItems())
	{
		BagItems.Add(Item);
	}
	const FDemoItem Finds[] = {
		{ TEXT("VenomweaveHelm"), EBeyondItemTier::Rare, 18, 0, false, false },
		{ TEXT("VenomweaveChest"), EBeyondItemTier::Epic, 20, 2, false, false },
		{ TEXT("IronboundHelm"), EBeyondItemTier::Uncommon, 14, 0, false, true },
		{ TEXT("IronboundBoots"), EBeyondItemTier::Common, 12, 0, false, true },
		{ TEXT("AcolytesChest"), EBeyondItemTier::Rare, 19, 1, false, false },
		{ TEXT("AcolytesGauntlets"), EBeyondItemTier::Uncommon, 16, 0, false, false },
		{ TEXT("GildedSaber"), EBeyondItemTier::Rare, 20, 3, false, false },
		{ TEXT("EclipseScepter"), EBeyondItemTier::Epic, 22, 0, true, false },
		{ TEXT("GlacierheartRod"), EBeyondItemTier::Rare, 19, 0, false, false },
		{ TEXT("Emberheart"), EBeyondItemTier::Legendary, 23, 0, true, false },
		{ TEXT("EmberglassRing"), EBeyondItemTier::Rare, 18, 0, false, false },
		{ TEXT("RimefrostLoop"), EBeyondItemTier::Uncommon, 15, 0, false, true },
		{ TEXT("VerdantCharm"), EBeyondItemTier::Epic, 21, 2, false, false },
		{ TEXT("ShadowveilPendant"), EBeyondItemTier::Rare, 19, 0, false, false },
	};
	for (const FDemoItem& Spec : Finds)
	{
		const FBeyondItemInstance Item = MakeDemoItem(Spec);
		if (Item.IsValid())
		{
			BagItems.Add(Item);
		}
	}
	Bag->RestoreItems(BagItems);
	Bag->RestoreWallet(FBeyondCurrencyBundle(20340, 1250, 85, 3));

	//~ Bond, story, the world
	Party->RestoreBondLevel(3, 64.0f);
	Party->AddBond(Party->MaxBond * 0.7f - Party->GetBond());
	Party->RestorePlaytime(32.0 * 3600.0 + 18.0 * 60.0 + 45.0);
	for (const TCHAR* Region : { TEXT("region_forest"), TEXT("village_mossbrook"), TEXT("village_fernhollow"), TEXT("village_brackenford"),
		TEXT("region_corrupted_woods"), TEXT("area_kingsfork") })
	{
		Party->DiscoverRegion(Region);
	}
	for (const TCHAR* Place : { TEXT("standing_stones"), TEXT("hollowroot_cave"), TEXT("old_watchtower"), TEXT("chapel_of_ash") })
	{
		Party->DiscoverPlace(Place);
	}
	Party->SetStoryFlag(TEXT("Talked.elder_maren.Elder_Intro"));
	Party->MarkBossDefeated(TEXT("gorehide"));
	if (UBeyondQuestComponent* Quests = PC->QuestComponent.Get())
	{
		for (const TCHAR* Done : { TEXT("ch1_mossbrook"), TEXT("ch2_pale_alpha"), TEXT("side_echoes_in_stone"), TEXT("side_thin_the_packs") })
		{
			Quests->CompleteQuest(Done, false);
		}
		Quests->SetTrackedQuest(TEXT("ch3_hollow_voice"));
	}
	// The wallet again: discovery paid into it
	Bag->RestoreWallet(FBeyondCurrencyBundle(20340, 1250, 85, 3));
	UE_LOG(LogBeyond, Log, TEXT("Beyond.UI.DemoState: party set up for screenshots"));
	return true;
}
