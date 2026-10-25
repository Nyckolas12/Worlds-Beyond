// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Editor.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondSaveGame.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondEconomySettings.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Items/BeyondLootSettings.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Progression/BeyondStatEffects.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Plan 6, gear and money: upgrade prices climb with the level (crystals from +5, echo shards from +10) and stop at the
 * maximum; upgrades raise every stat; salvage gives the tier's share plus half of what the upgrades cost; save v7 keeps
 * the wallet and the items' upgrade / lock / junk. In MAP_Demo_Main (Play In Editor): the party starts with the purse;
 * the wallet only spends what it has; a bag item and a worn weapon upgrade (the worn one's stats reach the demigod at
 * once); locked items can't be junk or salvaged; junk salvages into gold; a ring goes on the Accessory slot; a kill pays
 * its rank's gold. Saving is off for the run.
 */
namespace BeyondEconomyTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondEnemyCharacter> Wolf;
		TStrongObjectPtr<UBeyondItemDefinition> Ring;
		int32 GoldBeforeKill = 0;
		FBeyondCurrencyBundle KillReward;
		FString SavedSaveProgress;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	const UBeyondItemDefinition* FindDefinition(EBeyondItemSlot Slot)
	{
		const UBeyondItemDatabase* Database = UBeyondItemLibrary::GetItemDatabase();
		for (const UBeyondItemDefinition* Definition : Database ? Database->Items : TArray<TObjectPtr<UBeyondItemDefinition>>())
		{
			if (Definition && Definition->Slot == Slot && !Definition->BaseStats.IsEmpty() && !Definition->WeaponTag.IsValid())
			{
				return Definition;
			}
		}
		return nullptr;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondEconomyStep, TFunction<bool()>, Step);
bool FBeyondEconomyStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondEconomyTest, "WorldsBeyond.Prototype.Economy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondEconomyTest::RunTest(const FString& Parameters)
{
	using namespace BeyondEconomyTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;
	const UBeyondEconomySettings* Economy = GetDefault<UBeyondEconomySettings>();

	//~ Prices (no world needed)
	TestEqual(TEXT("+1 costs the tier's base gold"), Economy->GetUpgradeStepCost(EBeyondItemTier::Common, 1).Gold, Economy->UpgradeGold[0]);
	TestTrue(TEXT("Prices climb"), Economy->GetUpgradeStepCost(EBeyondItemTier::Common, 4).Gold > Economy->GetUpgradeStepCost(EBeyondItemTier::Common, 3).Gold);
	TestEqual(TEXT("No crystals before the crystal level"), Economy->GetUpgradeStepCost(EBeyondItemTier::Common, Economy->CrystalsFromUpgrade - 1).Crystals, 0);
	TestEqual(TEXT("Crystals from it"), Economy->GetUpgradeStepCost(EBeyondItemTier::Common, Economy->CrystalsFromUpgrade).Crystals, Economy->UpgradeCrystals[0]);
	TestEqual(TEXT("Echo shards from their level"), Economy->GetUpgradeStepCost(EBeyondItemTier::Common, Economy->EchoShardsFromUpgrade).EchoShards,
		Economy->UpgradeEchoShards[0]);
	TestTrue(TEXT("Legendary gear costs more"), Economy->GetUpgradeStepCost(EBeyondItemTier::Legendary, 1).Gold > Economy->GetUpgradeStepCost(EBeyondItemTier::Common, 1).Gold);
	TestTrue(TEXT("Nothing past the maximum"), Economy->GetUpgradeStepCost(EBeyondItemTier::Common, Economy->MaxUpgradeLevel + 1).IsEmpty());
	const FBeyondCurrencyBundle Steps = Economy->GetUpgradeTotalCost(EBeyondItemTier::Rare, 3);
	TestEqual(TEXT("The total is the sum of the steps"), Steps.Gold, Economy->GetUpgradeStepCost(EBeyondItemTier::Rare, 1).Gold
		+ Economy->GetUpgradeStepCost(EBeyondItemTier::Rare, 2).Gold + Economy->GetUpgradeStepCost(EBeyondItemTier::Rare, 3).Gold);
	const FString Formatted = UBeyondItemLibrary::FormatCurrencies(FBeyondCurrencyBundle(120, 3)).ToString();
	TestTrue(TEXT("Prices print as \"120 Gold, 3 Crystals\""), Formatted.Contains(TEXT("120 Gold")) && Formatted.Contains(TEXT("3 Crystals")));

	//~ Stats and salvage
	const UBeyondItemDefinition* Helm = FindDefinition(EBeyondItemSlot::Helm);
	if (TestNotNull(TEXT("A helm in the item database"), Helm))
	{
		const FBeyondItemInstance Plain = UBeyondItemLibrary::MakeItem(Helm, EBeyondItemTier::Common, 1);
		const FBeyondItemInstance Plus5 = UBeyondItemLibrary::WithUpgrade(Plain, 5);
		const FBeyondItemStat& Base = Helm->BaseStats[0];
		const float TierScale = GetDefault<UBeyondLootSettings>()->TierStatScale[static_cast<int32>(Plain.Tier)];
		TestEqual(TEXT("+5 is upgrade level 5"), Plus5.UpgradeLevel, 5);
		TestEqual(TEXT("+5: stats x (1 + 5 x upgrade stats)"), UBeyondItemLibrary::GetItemStat(Plus5, Base.Stat),
			FMath::RoundToFloat(Base.Value * TierScale * (1.0f + 5.0f * Economy->UpgradeStatsPerLevel)), 0.01f);
		TestEqual(TEXT("Upgrades stop at the maximum"), UBeyondItemLibrary::WithUpgrade(Plain, 99).UpgradeLevel, Economy->MaxUpgradeLevel);
		TestTrue(TEXT("...where the next upgrade has no price"), UBeyondItemLibrary::GetUpgradeCost(UBeyondItemLibrary::WithUpgrade(Plain, 99)).IsEmpty());
		const FBeyondCurrencyBundle PlainYield = UBeyondItemLibrary::GetSalvageYield(Plain);
		TestEqual(TEXT("Salvage: the tier's gold"), PlainYield.Gold, Economy->SalvageYield[static_cast<int32>(Plain.Tier)].Gold);
		TestEqual(TEXT("...plus half of what the upgrades cost"), UBeyondItemLibrary::GetSalvageYield(Plus5).Gold,
			PlainYield.Gold + FMath::FloorToInt(Economy->GetUpgradeTotalCost(Plain.Tier, 5).Gold * Economy->SalvageUpgradeRefund));

		//~ Save v7
		UBeyondSaveGame* Save = NewObject<UBeyondSaveGame>();
		FBeyondItemInstance Saved = Plus5;
		Saved.bLocked = true;
		Saved.bJunk = true;
		Save->Inventory.Add(Saved);
		Save->Wallet = FBeyondCurrencyBundle(1234, 5, 6, 7);
		Save->Members.FindOrAdd(TEXT("Test")).Equipped.Add(EBeyondItemSlot::Accessory, Plain);
		TArray<uint8> Bytes;
		const UBeyondSaveGame* Loaded = UGameplayStatics::SaveGameToMemory(Save, Bytes) ? Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
		TestTrue(TEXT("Save v7 keeps the wallet"), Loaded && Loaded->Version == 7 && Loaded->Wallet == FBeyondCurrencyBundle(1234, 5, 6, 7));
		TestTrue(TEXT("...the items' upgrade, lock and junk mark"), Loaded && Loaded->Inventory.Num() == 1 && Loaded->Inventory[0].UpgradeLevel == 5
			&& Loaded->Inventory[0].bLocked && Loaded->Inventory[0].bJunk);
		TestTrue(TEXT("...and the Accessory slot"), Loaded && Loaded->Members.Contains(TEXT("Test")) && Loaded->Members[TEXT("Test")].Equipped.Contains(EBeyondItemSlot::Accessory));
	}

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEconomyStep([State, Helm]()
	{
		FAutomationTestBase& T = *State->Test;
		const UBeyondEconomySettings* Economy = GetDefault<UBeyondEconomySettings>();
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		UBeyondInventoryComponent* Bag = PC ? PC->InventoryComponent.Get() : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller with a bag"), Bag) || !Helm)
		{
			return true;
		}
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (ULevelSequencePlayer* Player = It->GetSequencePlayer(); Player && Player->IsPlaying())
			{
				Player->GoToEndAndStop();
			}
		}
		for (ABeyondCharacterBase* Member : PC->PartyComponent->GetMembers())
		{
			if (Member->DuoRole == EBeyondDuoRole::Conduit)
			{
				State->Angel = Member;
			}
		}
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UBeyondEquipmentComponent* Equipment = Angel ? Angel->GetEquipmentComponent() : nullptr;
		if (!T.TestNotNull(TEXT("Angel with equipment"), Equipment))
		{
			return true;
		}
		UAbilitySystemComponent* AngelASC = Angel->GetAbilitySystemComponent();

		//~ Wallet
		T.TestTrue(TEXT("The party starts with the purse"), Bag->GetCurrency(EBeyondCurrency::Gold) >= Economy->StartingPurse.Gold);
		const FBeyondCurrencyBundle Start = Bag->GetWallet();
		T.TestFalse(TEXT("The wallet won't spend what it hasn't got"), Bag->Spend(FBeyondCurrencyBundle(Start.Gold + 1)));
		T.TestTrue(TEXT("...and nothing left it"), Bag->GetWallet() == Start);
		Bag->AddCurrencies(FBeyondCurrencyBundle(200000, 500, 200, 3));
		T.TestEqual(TEXT("Currency goes in"), Bag->GetCurrency(EBeyondCurrency::Gold), Start.Gold + 200000);

		//~ Upgrading a bag item
		const FBeyondItemInstance Loose = UBeyondItemLibrary::MakeItem(Helm, EBeyondItemTier::Common, 1);
		Bag->AddItem(Loose);
		const int32 GoldBefore = Bag->GetCurrency(EBeyondCurrency::Gold);
		FText Reason;
		T.TestTrue(TEXT("A bag item upgrades"), Bag->UpgradeItem(Loose.Id, Reason));
		FBeyondItemInstance Found;
		T.TestTrue(TEXT("...to +1"), Bag->FindItem(Loose.Id, Found) && Found.UpgradeLevel == 1);
		T.TestEqual(TEXT("...for its price"), Bag->GetCurrency(EBeyondCurrency::Gold), GoldBefore - UBeyondItemLibrary::GetUpgradeCost(Loose).Gold);

		//~ Upgrading what Angel wears: the stats reach him at once
		FBeyondItemInstance Staff;
		if (T.TestTrue(TEXT("Angel wears a weapon"), Equipment->GetEquipped(EBeyondItemSlot::Weapon, Staff)) && !UBeyondItemLibrary::GetItemStats(Staff).IsEmpty())
		{
			const EBeyondSkillStat Stat = UBeyondItemLibrary::GetItemStats(Staff)[0].Stat;
			const FGameplayAttribute Attribute = BeyondStats::GetStatAttribute(Stat);
			const float Before = AngelASC->GetNumericAttribute(Attribute);
			for (int32 Step = 0; Step < 6; ++Step)
			{
				Bag->UpgradeItem(Staff.Id, Reason);
			}
			FBeyondItemInstance Upgraded;
			T.TestTrue(TEXT("The worn staff is +6"), Equipment->GetEquipped(EBeyondItemSlot::Weapon, Upgraded) && Upgraded.UpgradeLevel == Staff.UpgradeLevel + 6);
			T.TestEqual(TEXT("...and Angel has its new stats"), AngelASC->GetNumericAttribute(Attribute),
				Before + UBeyondItemLibrary::GetItemStat(Upgraded, Stat) - UBeyondItemLibrary::GetItemStat(Staff, Stat), 0.01f);
			T.TestTrue(TEXT("...paid with crystals too from +5"), Bag->GetCurrency(EBeyondCurrency::Crystals) < 500 + Start.Crystals);
		}
		const FBeyondItemInstance Maxed = UBeyondItemLibrary::WithUpgrade(UBeyondItemLibrary::MakeItem(Helm, EBeyondItemTier::Common, 1), 99);
		Bag->AddItem(Maxed);
		T.TestFalse(TEXT("A maxed item can't go higher"), Bag->CanUpgradeItem(Maxed.Id, Reason));

		//~ Lock, junk, salvage
		T.TestTrue(TEXT("Lock the helm"), Bag->SetItemLocked(Loose.Id, true));
		FBeyondCurrencyBundle Yield;
		T.TestFalse(TEXT("A locked item won't salvage"), Bag->SalvageItem(Loose.Id, Yield));
		T.TestFalse(TEXT("...or become junk"), Bag->SetItemJunk(Loose.Id, true));
		T.TestTrue(TEXT("Unlock it"), Bag->SetItemLocked(Loose.Id, false));
		T.TestTrue(TEXT("Mark it and the maxed helm as junk"), Bag->SetItemJunk(Loose.Id, true) && Bag->SetItemJunk(Maxed.Id, true));
		T.TestEqual(TEXT("Two junk items"), Bag->GetJunkCount(), 2);
		FBeyondItemInstance LooseNow;
		Bag->FindItem(Loose.Id, LooseNow);
		const FBeyondCurrencyBundle Expected = [&]()
		{
			FBeyondCurrencyBundle Sum = UBeyondItemLibrary::GetSalvageYield(LooseNow);
			Sum += UBeyondItemLibrary::GetSalvageYield(Maxed);
			return Sum;
		}();
		const int32 GoldBeforeSalvage = Bag->GetCurrency(EBeyondCurrency::Gold);
		T.TestEqual(TEXT("Salvage the junk"), Bag->SalvageJunk(Yield), 2);
		T.TestTrue(TEXT("...for what both are worth"), Yield == Expected);
		T.TestEqual(TEXT("...into the wallet"), Bag->GetCurrency(EBeyondCurrency::Gold), GoldBeforeSalvage + Expected.Gold);
		T.TestFalse(TEXT("...and they are gone"), Bag->FindItem(Loose.Id, Found) || Bag->FindItem(Maxed.Id, Found));
		T.TestTrue(TEXT("Locking a worn item works too"), Bag->SetItemLocked(Staff.Id, true) && Equipment->GetEquipped(EBeyondItemSlot::Weapon, Found) && Found.bLocked);
		Bag->SetItemLocked(Staff.Id, false);

		//~ A ring on the Accessory slot
		UBeyondItemDefinition* Ring = NewObject<UBeyondItemDefinition>(GetTransientPackage());
		State->Ring.Reset(Ring);
		Ring->Slot = EBeyondItemSlot::Accessory;
		Ring->DisplayName = FText::FromString(TEXT("Test Ring"));
		FBeyondItemStat& RingStat = Ring->BaseStats.AddDefaulted_GetRef();
		RingStat.Stat = EBeyondSkillStat::CritRate;
		RingStat.Value = 4.0f;
		const float CritBefore = AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetCritRateAttribute());
		const FBeyondItemInstance RingItem = UBeyondItemLibrary::MakeItem(Ring, EBeyondItemTier::Common, 1);
		T.TestTrue(TEXT("A ring goes on the Accessory slot"), Equipment->EquipItem(RingItem) && Equipment->GetEquipped(EBeyondItemSlot::Accessory, Found));
		T.TestEqual(TEXT("...and adds its Critical Rate"), AngelASC->GetNumericAttribute(UCharacterAttributeSet::GetCritRateAttribute()),
			CritBefore + UBeyondItemLibrary::GetItemStat(RingItem, EBeyondSkillStat::CritRate), 0.01f);
		Equipment->Unequip(EBeyondItemSlot::Accessory);
		Bag->RemoveItem(RingItem.Id, Found);

		//~ A kill pays its rank's gold
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondEnemyDefinition* WolfDefinition = Roster ? Roster->FindEnemy(TEXT("forest_wolf")) : nullptr;
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		if (!T.TestNotNull(TEXT("forest_wolf in the roster"), WolfDefinition) || !Enemies)
		{
			return true;
		}
		FBeyondEnemySpawnParams Params;
		Params.Level = 3;
		ABeyondEnemyCharacter* Wolf = Enemies->SpawnEnemy(WolfDefinition, FTransform(Angel->GetActorLocation() + Angel->GetActorForwardVector() * 800.0f), Params);
		if (!T.TestNotNull(TEXT("A wolf"), Wolf))
		{
			return true;
		}
		State->Wolf = Wolf;
		if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Wolf->GetController()))
		{
			AI->SetBrainEnabled(false);
		}
		State->KillReward = Economy->GetKillReward(Wolf->Rank, Wolf->GetCharacterLevel());
		State->GoldBeforeKill = Bag->GetCurrency(EBeyondCurrency::Gold);
		T.TestTrue(TEXT("A level 3 kill pays more than a level 1 one"), State->KillReward.Gold > Economy->GetKillReward(Wolf->Rank, 1).Gold);
		UBeyondCombatLibrary::ApplyDamage(Angel, Wolf, 1000000.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true, true);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEconomyStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		const ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const UBeyondInventoryComponent* Bag = PC ? PC->InventoryComponent.Get() : nullptr;
		if (Bag && State->KillReward.Gold > 0)
		{
			T.TestEqual(TEXT("The kill paid its gold"), Bag->GetCurrency(EBeyondCurrency::Gold) - State->GoldBeforeKill, State->KillReward.Gold);
		}
		if (ABeyondEnemyCharacter* Wolf = State->Wolf.Get())
		{
			Wolf->Destroy();
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondEconomyStep([State]()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			return false;
		}
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		return true;
	}));
	return true;
}

#endif
