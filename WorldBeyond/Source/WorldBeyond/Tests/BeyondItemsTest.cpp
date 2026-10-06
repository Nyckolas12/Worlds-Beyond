// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "BrainComponent.h"
#include "CharacterAttributeSet.h"
#include "Characters/BeyondCharacterBase.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Game/BeyondSaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Items/BeyondLootDrop.h"
#include "Items/BeyondLootSettings.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondInventoryWidget.h"
#include "UI/BeyondProgressWidget.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Plan 2 in MAP_Demo_Main (Play In Editor): the item database from migrate_pass9.py, tier / level scaling, the starter
 * kit, equipping (stats, weapon types, swaps), 2-piece bonuses and the three full-set effects (on test sets with every
 * proc forced), loot rolls and drops from a boss-ranked kill, picking up with F (and a full bag), the I screen and
 * items in the save game. Saving is switched off for the run (Beyond.SaveProgress 0) so the player's save is untouched.
 *
 * UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash
 */

namespace BeyondItemsTest
{
	const EBeyondItemSlot ArmorSlots[] = { EBeyondItemSlot::Helm, EBeyondItemSlot::Chest, EBeyondItemSlot::Gauntlets, EBeyondItemSlot::Boots };

	// A test armor set and its four pieces (kept alive for the whole run)
	struct FItemsTestSet
	{
		TStrongObjectPtr<UBeyondArmorSet> Set;
		TArray<TStrongObjectPtr<UBeyondItemDefinition>> Pieces;
	};

	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TWeakObjectPtr<ABeyondPlayerController> PC;
		TWeakObjectPtr<ABeyondCharacterBase> Angel;
		TWeakObjectPtr<ABeyondCharacterBase> JiWoong;
		TArray<TWeakObjectPtr<ABeyondCharacterBase>> Enemies;
		TMap<FString, float> Numbers;
		FString SavedSaveProgress;
		TWeakObjectPtr<AAIController> Holder;
		TWeakObjectPtr<AController> CompanionController;

		FItemsTestSet Venom;
		FItemsTestSet Storm;
		FItemsTestSet Sun;
		TStrongObjectPtr<UBeyondItemDefinition> Plain;
		TWeakObjectPtr<ABeyondLootDrop> PickupDrop;
		FGuid PickupItem;
	};

	UWorld* GetItemsPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	float ReadItemsStat(const ABeyondCharacterBase* Character, const FGameplayAttribute& Attribute)
	{
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC ? ASC->GetNumericAttribute(Attribute) : 0.0f;
	}

	void WriteItemsBaseStat(ABeyondCharacterBase* Character, const FGameplayAttribute& Attribute, float Value)
	{
		if (UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr)
		{
			ASC->SetNumericAttributeBase(Attribute, Value);
		}
	}

	// Stops an enemy's AI, defences and movement; lots of health and no Defense so hits are easy to measure
	void HoldEnemyForItems(ABeyondCharacterBase* Enemy)
	{
		if (!Enemy)
		{
			return;
		}
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Items test"));
			}
		}
		if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Blocking, 0);
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Parrying, 0);
			ASC->SetLooseGameplayTagCount(BeyondTags::State_Invincible, 0);
		}
		WriteItemsBaseStat(Enemy, UCharacterAttributeSet::GetMaxHealthAttribute(), 100000.0f);
		WriteItemsBaseStat(Enemy, UCharacterAttributeSet::GetCurrentHealthAttribute(), 100000.0f);
		WriteItemsBaseStat(Enemy, UCharacterAttributeSet::GetDefenseAttribute(), 0.0f);
		Enemy->GetCharacterMovement()->StopMovementImmediately();
	}

	FItemsTestSet MakeItemsTestSet(const TCHAR* Name, EBeyondSetEffect Effect, const FBeyondSetEffectSettings& Settings)
	{
		FItemsTestSet Result;
		Result.Set.Reset(NewObject<UBeyondArmorSet>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UBeyondArmorSet::StaticClass(), FName(*FString::Printf(TEXT("TestSet_%s"), Name)))));
		Result.Set->SetName = FText::FromString(Name);
		Result.Set->FullSetEffect = Effect;
		Result.Set->Effect = Settings;
		FBeyondItemStat& TwoPiece = Result.Set->TwoPieceStats.AddDefaulted_GetRef();
		TwoPiece.Stat = EBeyondSkillStat::Strength;
		TwoPiece.Value = 5.0f;

		for (const EBeyondItemSlot Slot : ArmorSlots)
		{
			UBeyondItemDefinition* Piece = NewObject<UBeyondItemDefinition>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(),
				UBeyondItemDefinition::StaticClass(), FName(*FString::Printf(TEXT("TestPiece_%s_%d"), Name, static_cast<int32>(Slot)))));
			Piece->DisplayName = FText::FromString(FString::Printf(TEXT("%s piece"), Name));
			Piece->Slot = Slot;
			Piece->Set = Result.Set.Get();
			// Stamina only, so Defense (and the damage maths) stays the same whichever set is worn
			FBeyondItemStat& Stat = Piece->BaseStats.AddDefaulted_GetRef();
			Stat.Stat = EBeyondSkillStat::MaxStamina;
			Stat.Value = 2.0f;
			Result.Pieces.Emplace(Piece);
		}
		return Result;
	}

	bool WearItemsTestSet(UBeyondEquipmentComponent* Equipment, const FItemsTestSet& TestSet, int32 Count)
	{
		bool bAll = Equipment != nullptr;
		for (int32 Index = 0; Equipment && Index < FMath::Min(Count, TestSet.Pieces.Num()); ++Index)
		{
			bAll &= Equipment->EquipItem(UBeyondItemLibrary::MakeItem(TestSet.Pieces[Index].Get(), EBeyondItemTier::Rare, 1));
		}
		return bAll;
	}

	int32 CountDamageOverTime(const AActor* Target)
	{
		const UAbilitySystemComponent* ASC = UBeyondCombatLibrary::GetASC(Target);
		if (!ASC)
		{
			return 0;
		}
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UBeyondGE_DamageOverTime::StaticClass();
		return ASC->GetActiveEffects(Query).Num();
	}

	// Ends any poison / burn so a measurement starts clean
	void ClearItemsDamageOverTime(const AActor* Target)
	{
		if (UAbilitySystemComponent* ASC = UBeyondCombatLibrary::GetASC(Target))
		{
			FGameplayEffectQuery Query;
			Query.EffectDefinition = UBeyondGE_DamageOverTime::StaticClass();
			ASC->RemoveActiveEffects(Query);
		}
	}

	const UBeyondItemDefinition* FindDatabaseItem(const UBeyondItemDatabase* Database, TFunctionRef<bool(const UBeyondItemDefinition&)> Filter)
	{
		for (const UBeyondItemDefinition* Item : Database ? Database->Items : TArray<TObjectPtr<UBeyondItemDefinition>>())
		{
			if (Item && Filter(*Item))
			{
				return Item;
			}
		}
		return nullptr;
	}

	// An environment hit from Source on Angel at full / given health; returns the health it took
	float MeasureHitOnWearer(ABeyondCharacterBase* Source, ABeyondCharacterBase* Wearer, float Amount, float HealthFraction)
	{
		UAbilitySystemComponent* ASC = Wearer->GetAbilitySystemComponent();
		ASC->SetLooseGameplayTagCount(BeyondTags::State_Invincible, 0);
		const float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), MaxHealth * HealthFraction);
		const float Before = UBeyondCombatLibrary::GetActorHealth(Wearer);
		UBeyondCombatLibrary::ApplyDamage(Source, Wearer, Amount, BeyondTags::DamageType_Environment, FGameplayTag(), true, nullptr, true, true);
		const float Taken = Before - UBeyondCombatLibrary::GetActorHealth(Wearer);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(), MaxHealth);
		return Taken;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondItemsStep, TFunction<bool()>, Step);
bool FBeyondItemsStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondItemsTest, "WorldsBeyond.Prototype.Items",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondItemsTest::RunTest(const FString& Parameters)
{
	using namespace BeyondItemsTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}
	else
	{
		AddError(TEXT("Beyond.SaveProgress console variable is missing"));
	}

	// Test sets: every proc forced so the checks are deterministic
	{
		FBeyondSetEffectSettings Venom;
		Venom.InfusionDuration = 6.0f;
		Venom.InfusionCooldown = 12.0f;
		Venom.DamagePerSecond = 8.0f;
		Venom.DotDuration = 4.0f;
		State->Venom = MakeItemsTestSet(TEXT("Venom"), EBeyondSetEffect::VenomInfusion, Venom);

		FBeyondSetEffectSettings Storm;
		Storm.ProcChance = 1.0f;
		Storm.ProcCooldown = 0.0f;
		Storm.ProcDamage = 20.0f;
		Storm.Radius = 600.0f;
		Storm.MaxTargets = 3;
		State->Storm = MakeItemsTestSet(TEXT("Storm"), EBeyondSetEffect::ChainLightning, Storm);

		FBeyondSetEffectSettings Sun;
		Sun.DamagePerSecond = 6.0f;
		Sun.DotDuration = 3.0f;
		Sun.DamageTakenReduction = 0.15f;
		Sun.ReductionHealthThreshold = 0.5f;
		State->Sun = MakeItemsTestSet(TEXT("Sun"), EBeyondSetEffect::Sunfire, Sun);

		// A plain helm for the scaling and equip checks
		State->Plain.Reset(NewObject<UBeyondItemDefinition>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UBeyondItemDefinition::StaticClass(), TEXT("TestPlainHelm"))));
		State->Plain->DisplayName = FText::FromString(TEXT("Test Helm"));
		State->Plain->Slot = EBeyondItemSlot::Helm;
		FBeyondItemStat& Health = State->Plain->BaseStats.AddDefaulted_GetRef();
		Health.Stat = EBeyondSkillStat::MaxHealth;
		Health.Value = 20.0f;
		FBeyondItemStat& Defense = State->Plain->BaseStats.AddDefaulted_GetRef();
		Defense.Stat = EBeyondSkillStat::Defense;
		Defense.Value = 4.0f;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	// Setup, the database from migrate_pass9.py, scaling and the starter kit
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetItemsPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!T.TestNotNull(TEXT("Beyond player controller"), PC))
		{
			return true;
		}
		State->PC = PC;
		UBeyondPartyComponent* Party = PC->PartyComponent;
		T.TestFalse(TEXT("Saving is off for the test run"), Party->IsSavingEnabled());
		T.TestNotNull(TEXT("The player controller has the party's bag"), PC->InventoryComponent.Get());

		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (ULevelSequencePlayer* Player = It->GetSequencePlayer(); Player && Player->IsPlaying())
			{
				Player->GoToEndAndStop();
			}
		}

		for (ABeyondCharacterBase* Member : Party->GetMembers())
		{
			if (Member->DuoRole == EBeyondDuoRole::Conduit) { State->Angel = Member; }
			if (Member->DuoRole == EBeyondDuoRole::Striker) { State->JiWoong = Member; }
		}
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!T.TestNotNull(TEXT("Angel"), Angel) || !T.TestNotNull(TEXT("Ji-Woong"), JiWoong))
		{
			return true;
		}
		if (Party->GetLeader() != Angel)
		{
			Party->SwapLeader();
		}
		State->CompanionController = JiWoong->GetController();
		if (AAIController* Holder = World->SpawnActor<AAIController>())
		{
			Holder->Possess(JiWoong);
			State->Holder = Holder;
		}
		T.TestNotNull(TEXT("Angel has an equipment component"), Angel->GetEquipmentComponent());
		T.TestNotNull(TEXT("Ji-Woong has an equipment component"), JiWoong->GetEquipmentComponent());

		for (TActorIterator<ABeyondCharacterBase> It(World); It; ++It)
		{
			if (It->TeamAffiliation == EBeyondTeam::Enemy && !UBeyondCombatLibrary::IsActorDead(*It) && !It->BossBarWidgetClass)
			{
				T.TestNull(*FString::Printf(TEXT("Enemy %s wears nothing (no equipment component)"), *It->GetName()), It->GetEquipmentComponent());
				HoldEnemyForItems(*It);
				State->Enemies.Add(*It);
			}
		}
		T.TestTrue(TEXT("At least 2 regular enemies in the map"), State->Enemies.Num() >= 2);

		//~ Database (migrate_pass9.py)
		const UBeyondItemDatabase* Database = UBeyondItemLibrary::GetItemDatabase();
		if (T.TestNotNull(TEXT("migrate_pass9: item database (Project Settings -> Worlds Beyond Loot)"), Database))
		{
			T.TestTrue(TEXT("migrate_pass9: at least 30 items"), Database->Items.Num() >= 30);
			TMap<const UBeyondArmorSet*, TSet<EBeyondItemSlot>> SetSlots;
			int32 Swords = 0;
			int32 Staves = 0;
			for (const UBeyondItemDefinition* Item : Database->Items)
			{
				if (!T.TestNotNull(TEXT("Database entry"), Item))
				{
					continue;
				}
				const FString Name = Item->GetName();
				T.TestFalse(*FString::Printf(TEXT("%s has a name"), *Name), Item->DisplayName.IsEmpty());
				T.TestTrue(*FString::Printf(TEXT("%s has stats"), *Name), Item->BaseStats.Num() > 0);
				T.TestTrue(*FString::Printf(TEXT("%s: min tier <= max tier"), *Name), Item->MinTier <= Item->MaxTier);
				if (Item->Slot == EBeyondItemSlot::Weapon)
				{
					T.TestTrue(*FString::Printf(TEXT("%s (weapon) has a weapon type"), *Name), Item->WeaponTag.IsValid());
					Swords += Item->WeaponTag.ToString().Contains(TEXT("Sword")) ? 1 : 0;
					Staves += Item->WeaponTag.ToString().Contains(TEXT("Staff")) ? 1 : 0;
					T.TestNull(*FString::Printf(TEXT("%s (weapon) is in no armor set"), *Name), Item->Set.Get());
				}
				if (Item->Set)
				{
					SetSlots.FindOrAdd(Item->Set).Add(Item->Slot);
				}
			}
			T.TestTrue(TEXT("migrate_pass9: at least three armor sets"), SetSlots.Num() >= 3);
			for (const TPair<const UBeyondArmorSet*, TSet<EBeyondItemSlot>>& Pair : SetSlots)
			{
				const FString SetName = Pair.Key->SetName.ToString();
				T.TestEqual(*FString::Printf(TEXT("%s: one piece per armor slot"), *SetName), Pair.Value.Num(), UBeyondArmorSet::FullSetPieces);
				T.TestTrue(*FString::Printf(TEXT("%s has a full-set effect"), *SetName), Pair.Key->FullSetEffect != EBeyondSetEffect::None);
				T.TestTrue(*FString::Printf(TEXT("%s has a 2-piece bonus"), *SetName), Pair.Key->TwoPieceStats.Num() > 0);
			}
			T.TestTrue(TEXT("migrate_pass9: swords for Ji-Woong and staves for Angel"), Swords >= 2 && Staves >= 2);
		}

		//~ Tier and level scaling, bonus stats
		const UBeyondItemDefinition* Plain = State->Plain.Get();
		const TArray<float>& TierScale = GetDefault<UBeyondLootSettings>()->TierStatScale;
		const float PerLevel = GetDefault<UBeyondLootSettings>()->StatsPerItemLevel;
		const FBeyondItemInstance Common = UBeyondItemLibrary::MakeItem(Plain, EBeyondItemTier::Common, 1);
		const FBeyondItemInstance Rare = UBeyondItemLibrary::MakeItem(Plain, EBeyondItemTier::Rare, 1);
		const FBeyondItemInstance Leveled = UBeyondItemLibrary::MakeItem(Plain, EBeyondItemTier::Common, 5);
		T.TestTrue(TEXT("A new item is valid and has an id"), Common.IsValid() && Common.Id != Rare.Id);
		T.TestEqual(TEXT("Common, item level 1: base stats"), UBeyondItemLibrary::GetItemStat(Common, EBeyondSkillStat::MaxHealth), 20.0f, 0.01f);
		T.TestEqual(TEXT("Rare scales stats by its tier"), UBeyondItemLibrary::GetItemStat(Rare, EBeyondSkillStat::MaxHealth),
			FMath::RoundToFloat(20.0f * TierScale[2]), 0.01f);
		T.TestEqual(TEXT("Item level 5 adds 4 levels of growth"), UBeyondItemLibrary::GetItemStat(Leveled, EBeyondSkillStat::MaxHealth),
			FMath::RoundToFloat(20.0f * (1.0f + PerLevel * 4.0f)), 0.01f);
		FRandomStream Stream(42);
		T.TestEqual(TEXT("Epic items roll one bonus stat"), UBeyondItemLibrary::MakeItemFromStream(Plain, EBeyondItemTier::Epic, 1, Stream).BonusStats.Num(), 1);
		T.TestEqual(TEXT("Legendary items roll two"), UBeyondItemLibrary::MakeItemFromStream(Plain, EBeyondItemTier::Legendary, 1, Stream).BonusStats.Num(), 2);
		const FBeyondItemInstance SetPiece = UBeyondItemLibrary::MakeItem(State->Venom.Pieces[0].Get(), EBeyondItemTier::Common, 1);
		T.TestTrue(TEXT("Set pieces are never below the minimum set tier"), SetPiece.Tier >= GetDefault<UBeyondLootSettings>()->MinSetTier);

		//~ Starter kit
		for (const ABeyondCharacterBase* Member : { static_cast<const ABeyondCharacterBase*>(Angel), static_cast<const ABeyondCharacterBase*>(JiWoong) })
		{
			FBeyondItemInstance Weapon;
			const UBeyondEquipmentComponent* Equipment = Member->GetEquipmentComponent();
			if (Equipment && T.TestTrue(*FString::Printf(TEXT("Starter kit: %s holds a weapon"), *Member->GetName()), Equipment->GetEquipped(EBeyondItemSlot::Weapon, Weapon)))
			{
				FText Reason;
				T.TestTrue(*FString::Printf(TEXT("Starter kit: %s can wield the weapon they hold"), *Member->GetName()), Equipment->CanEquip(Weapon, Reason));
			}
		}
		return true;
	}));

	// Equipping: stats go up, the other demigod's weapon type is refused, swapping returns the old item, taking off removes the stats
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		UBeyondEquipmentComponent* Equipment = Angel ? Angel->GetEquipmentComponent() : nullptr;
		UBeyondInventoryComponent* Bag = PC ? PC->InventoryComponent.Get() : nullptr;
		if (!Angel || !JiWoong || !Equipment || !Bag)
		{
			return true;
		}

		// Start bare-headed
		Equipment->Unequip(EBeyondItemSlot::Helm);
		const float Health = ReadItemsStat(Angel, UCharacterAttributeSet::GetMaxHealthAttribute());
		const float Defense = ReadItemsStat(Angel, UCharacterAttributeSet::GetDefenseAttribute());
		const float GearDefense = Equipment->GetStatBonus(EBeyondSkillStat::Defense);

		const FBeyondItemInstance Helm = UBeyondItemLibrary::MakeItem(State->Plain.Get(), EBeyondItemTier::Common, 1);
		T.TestTrue(TEXT("Item goes into the bag"), Bag->AddItem(Helm));
		T.TestTrue(TEXT("Equip from the bag"), Equipment->EquipFromInventory(Helm.Id));
		FBeyondItemInstance Found;
		T.TestFalse(TEXT("The equipped item left the bag"), Bag->FindItem(Helm.Id, Found));
		T.TestEqual(TEXT("Equipping raises max health by the item's stat"), ReadItemsStat(Angel, UCharacterAttributeSet::GetMaxHealthAttribute()) - Health, 20.0f, 0.01f);
		T.TestEqual(TEXT("Equipping raises Defense by the item's stat"), ReadItemsStat(Angel, UCharacterAttributeSet::GetDefenseAttribute()) - Defense, 4.0f, 0.01f);
		T.TestEqual(TEXT("Equipment reports its share"), Equipment->GetStatBonus(EBeyondSkillStat::Defense) - GearDefense, 4.0f, 0.01f);

		// Swapping: the old helm goes back to the bag
		const FBeyondItemInstance Better = UBeyondItemLibrary::MakeItem(State->Plain.Get(), EBeyondItemTier::Rare, 1);
		Bag->AddItem(Better);
		T.TestTrue(TEXT("Equip a better helm"), Equipment->EquipFromInventory(Better.Id));
		T.TestTrue(TEXT("The old helm is back in the bag"), Bag->FindItem(Helm.Id, Found));
		FBeyondItemInstance Worn;
		T.TestTrue(TEXT("The new helm is worn"), Equipment->GetEquipped(EBeyondItemSlot::Helm, Worn) && Worn.Id == Better.Id);

		// Taking it off removes its stats
		T.TestTrue(TEXT("Take the helm off"), Equipment->Unequip(EBeyondItemSlot::Helm));
		T.TestTrue(TEXT("It's in the bag"), Bag->FindItem(Better.Id, Found));
		T.TestEqual(TEXT("Its stats are gone"), ReadItemsStat(Angel, UCharacterAttributeSet::GetMaxHealthAttribute()), Health, 0.01f);

		// Weapon types: a sword fits Ji-Woong, not Angel
		const UBeyondItemDefinition* Sword = FindDatabaseItem(UBeyondItemLibrary::GetItemDatabase(), [](const UBeyondItemDefinition& Item)
		{
			return Item.Slot == EBeyondItemSlot::Weapon && Item.WeaponTag.ToString().Contains(TEXT("Sword"));
		});
		if (T.TestNotNull(TEXT("migrate_pass9: a sword in the database"), Sword))
		{
			const FBeyondItemInstance Blade = UBeyondItemLibrary::MakeItem(Sword, EBeyondItemTier::Uncommon, 1);
			Bag->AddItem(Blade);
			FText Reason;
			T.TestFalse(TEXT("Angel can't wield a sword"), Equipment->CanEquip(Blade, Reason));
			T.TestFalse(TEXT("The reason says so"), Reason.IsEmpty());
			T.TestFalse(TEXT("Equipping it on Angel is refused"), Equipment->EquipFromInventory(Blade.Id));
			T.TestTrue(TEXT("The refused sword stays in the bag"), Bag->FindItem(Blade.Id, Found));
			T.TestTrue(TEXT("Ji-Woong can wield it"), JiWoong->GetEquipmentComponent()->EquipFromInventory(Blade.Id));
		}

		// Clear Angel's armor for the set checks (into the bag)
		for (const EBeyondItemSlot Slot : ArmorSlots)
		{
			Equipment->Unequip(Slot);
		}
		State->Numbers.Add(TEXT("Strength"), ReadItemsStat(Angel, UCharacterAttributeSet::GetStrengthAttribute()));
		return true;
	}));

	// Venomweave (test set): 2 pieces give the bonus, 4 switch the effect on; Q starts the infusion
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		UBeyondEquipmentComponent* Equipment = Angel ? Angel->GetEquipmentComponent() : nullptr;
		if (!Equipment || State->Enemies.Num() < 2)
		{
			return true;
		}

		T.TestTrue(TEXT("Wear 2 Venom pieces"), WearItemsTestSet(Equipment, State->Venom, 2));
		T.TestEqual(TEXT("2 pieces counted"), Equipment->GetSetPieceCount(State->Venom.Set.Get()), 2);
		T.TestEqual(TEXT("2-piece bonus: +5 Strength"), ReadItemsStat(Angel, UCharacterAttributeSet::GetStrengthAttribute()) - State->Numbers.FindRef(TEXT("Strength")), 5.0f, 0.01f);
		T.TestFalse(TEXT("No full-set effect at 2 pieces"), Equipment->HasFullSet(EBeyondSetEffect::VenomInfusion));
		T.TestFalse(TEXT("No infusion without the full set"), Equipment->StartInfusion());

		// Pieces 3 and 4
		for (int32 Index = 2; Index < 4; ++Index)
		{
			Equipment->EquipItem(UBeyondItemLibrary::MakeItem(State->Venom.Pieces[Index].Get(), EBeyondItemTier::Rare, 1));
		}
		T.TestTrue(TEXT("4 pieces: Venom Infusion is on"), Equipment->HasFullSet(EBeyondSetEffect::VenomInfusion));

		// Using the Q ability coats the weapon
		UAbilitySystemComponent* ASC = Angel->GetAbilitySystemComponent();
		const FGameplayTag InputRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input"));
		FGameplayAbilitySpecHandle QHandle;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UBeyondGameplayAbility* Beyond = Cast<UBeyondGameplayAbility>(Spec.Ability);
			const bool bHasInput = Spec.GetDynamicSpecSourceTags().HasTag(InputRoot);
			if (bHasInput ? Spec.GetDynamicSpecSourceTags().HasTagExact(BeyondTags::Ability_Input_Q) : (Beyond && Beyond->InputTag == BeyondTags::Ability_Input_Q))
			{
				QHandle = Spec.Handle;
				break;
			}
		}
		const bool bUsedQ = QHandle.IsValid() && ASC->TryActivateAbility(QHandle);
		if (bUsedQ)
		{
			T.TestTrue(TEXT("Using Q starts Venom Infusion"), Equipment->IsInfused());
		}
		else
		{
			T.AddWarning(TEXT("Angel's Q could not be used here; Venom Infusion started directly instead"));
			T.TestTrue(TEXT("Venom Infusion starts"), Equipment->StartInfusion());
		}
		T.TestTrue(TEXT("Infused"), Equipment->IsInfused());
		T.TestFalse(TEXT("The infusion has a cooldown"), Equipment->StartInfusion());

		// An infused hit poisons
		ABeyondCharacterBase* First = State->Enemies[0].Get();
		HoldEnemyForItems(First);
		ClearItemsDamageOverTime(First);
		UBeyondCombatLibrary::ApplyDamage(Angel, First, 10.0f, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Light, true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* First = State->Enemies.Num() > 1 ? State->Enemies[0].Get() : nullptr;
		if (!First)
		{
			return true;
		}
		T.TestEqual(TEXT("The infused hit poisoned the enemy"), CountDamageOverTime(First), 1);
		State->Numbers.Add(TEXT("PoisonedHealth"), UBeyondCombatLibrary::GetActorHealth(First));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* First = State->Enemies.Num() > 1 ? State->Enemies[0].Get() : nullptr;
		ABeyondCharacterBase* Second = State->Enemies.Num() > 1 ? State->Enemies[1].Get() : nullptr;
		UBeyondEquipmentComponent* Equipment = Angel ? Angel->GetEquipmentComponent() : nullptr;
		if (!First || !Second || !Equipment)
		{
			return true;
		}
		T.TestTrue(TEXT("The poison ticks"), UBeyondCombatLibrary::GetActorHealth(First) < State->Numbers.FindRef(TEXT("PoisonedHealth")) - 1.0f);

		// Breaking the set ends the infusion; Stormforged (test set) goes on instead
		Equipment->Unequip(EBeyondItemSlot::Boots);
		T.TestFalse(TEXT("3 pieces: the full-set effect is off"), Equipment->HasFullSet(EBeyondSetEffect::VenomInfusion));
		T.TestFalse(TEXT("Taking a piece off ends the infusion"), Equipment->IsInfused());
		T.TestTrue(TEXT("Wear 4 Storm pieces"), WearItemsTestSet(Equipment, State->Storm, 4));
		T.TestTrue(TEXT("Chain Lightning is on"), Equipment->HasFullSet(EBeyondSetEffect::ChainLightning));
		T.TestEqual(TEXT("No Venom pieces left on"), Equipment->GetSetPieceCount(State->Venom.Set.Get()), 0);

		// Stand the second enemy next to the first. Set-effect damage (DamageType.Proc.*) never triggers set effects:
		// a lightning-tagged hit on the first must not arc
		Second->SetActorLocation(First->GetActorLocation() + FVector(0.0f, 250.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		HoldEnemyForItems(Second);
		ClearItemsDamageOverTime(Second);
		State->Numbers.Add(TEXT("SecondHealth"), UBeyondCombatLibrary::GetActorHealth(Second));
		UBeyondCombatLibrary::ApplyDamage(Angel, First, 5.0f, BeyondTags::DamageType_Proc_Lightning, FGameplayTag(), true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* First = State->Enemies.Num() > 1 ? State->Enemies[0].Get() : nullptr;
		const ABeyondCharacterBase* Second = State->Enemies.Num() > 1 ? State->Enemies[1].Get() : nullptr;
		if (!Angel || !First || !Second)
		{
			return true;
		}
		T.TestEqual(TEXT("Set-effect damage doesn't set off set effects (no chain from a proc)"),
			State->Numbers.FindRef(TEXT("SecondHealth")) - UBeyondCombatLibrary::GetActorHealth(Second), 0.0f, 0.01f);

		// A real hit on the first arcs to the second
		State->Numbers.Add(TEXT("SecondHealth"), UBeyondCombatLibrary::GetActorHealth(Second));
		UBeyondCombatLibrary::ApplyDamage(Angel, First, 10.0f, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Light, true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondCharacterBase* Second = State->Enemies.Num() > 1 ? State->Enemies[1].Get() : nullptr;
		UBeyondEquipmentComponent* Equipment = Angel ? Angel->GetEquipmentComponent() : nullptr;
		if (!Second || !Equipment || State->Enemies.Num() < 1)
		{
			return true;
		}
		const float ProcScale = 1.0f + 0.1f * (Angel->GetCharacterLevel() - 1);
		T.TestEqual(TEXT("Chain lightning struck the nearby enemy"), State->Numbers.FindRef(TEXT("SecondHealth")) - UBeyondCombatLibrary::GetActorHealth(Second),
			20.0f * ProcScale, 1.0f);

		// Sunforged (test set): less damage taken above half health
		ABeyondCharacterBase* Attacker = State->Enemies[0].Get();
		T.TestTrue(TEXT("Wear 4 Sun pieces"), WearItemsTestSet(Equipment, State->Sun, 4));
		T.TestTrue(TEXT("Sunfire is on"), Equipment->HasFullSet(EBeyondSetEffect::Sunfire));
		const float Healthy = MeasureHitOnWearer(Attacker, Angel, 20.0f, 1.0f);
		const float Hurt = MeasureHitOnWearer(Attacker, Angel, 20.0f, 0.4f);
		T.TestTrue(TEXT("Hits land on the wearer"), Hurt > 0.0f);
		T.TestEqual(TEXT("Sunfire: -15 % damage taken above half health"), Healthy, Hurt * 0.85f, 0.3f);

		// Hits burn
		HoldEnemyForItems(Second);
		ClearItemsDamageOverTime(Second);
		UBeyondCombatLibrary::ApplyDamage(Angel, Second, 10.0f, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Light, true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		const ABeyondCharacterBase* Second = State->Enemies.Num() > 1 ? State->Enemies[1].Get() : nullptr;
		if (!Second)
		{
			return true;
		}
		T.TestEqual(TEXT("Sunfire set the enemy ablaze"), CountDamageOverTime(Second), 1);
		State->Numbers.Add(TEXT("BurningHealth"), UBeyondCombatLibrary::GetActorHealth(Second));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		const ABeyondCharacterBase* Second = State->Enemies.Num() > 1 ? State->Enemies[1].Get() : nullptr;
		if (!Angel || !Second)
		{
			return true;
		}
		T.TestTrue(TEXT("The burn ticks"), UBeyondCombatLibrary::GetActorHealth(Second) < State->Numbers.FindRef(TEXT("BurningHealth")) - 1.0f);

		// Loot: a boss-ranked enemy rolls 3 items with a set piece and a weapon...
		ABeyondCharacterBase* StandIn = State->Enemies.Num() > 2 ? State->Enemies[2].Get() : State->Enemies[0].Get();
		UBeyondLootSubsystem* Loot = UBeyondLootSubsystem::Get(Angel);
		if (!StandIn || !T.TestNotNull(TEXT("Loot subsystem"), Loot))
		{
			return true;
		}
		StandIn->Rank = EBeyondEnemyRank::Boss;
		const TArray<FBeyondItemInstance> Rolled = UBeyondItemLibrary::RollLootForEnemy(StandIn, 7);
		T.TestEqual(TEXT("A boss drops 3 items"), Rolled.Num(), 3 + StandIn->GuaranteedLoot.Num());
		T.TestTrue(TEXT("...one of them a set piece"), Rolled.ContainsByPredicate([](const FBeyondItemInstance& Item)
		{
			const UBeyondItemDefinition* Definition = Item.GetDefinition();
			return Definition && Definition->Set;
		}));
		T.TestTrue(TEXT("...and one a weapon"), Rolled.ContainsByPredicate([](const FBeyondItemInstance& Item)
		{
			const UBeyondItemDefinition* Definition = Item.GetDefinition();
			return Definition && Definition->Slot == EBeyondItemSlot::Weapon;
		}));
		for (const FBeyondItemInstance& Item : Rolled)
		{
			T.TestEqual(TEXT("Loot is at the enemy's level"), Item.ItemLevel, StandIn->GetCharacterLevel());
		}

		// ...and killing it drops them on the ground
		State->Numbers.Add(TEXT("Drops"), static_cast<float>(Loot->GetDropCount()));
		HoldEnemyForItems(StandIn);
		UBeyondCombatLibrary::ApplyDamage(Angel, StandIn, 10000000.0f, BeyondTags::DamageType_Projectile, BeyondTags::Event_Hit_Light, true, nullptr, true, true);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* Angel = State->Angel.Get();
		ABeyondPlayerController* PC = State->PC.Get();
		UBeyondLootSubsystem* Loot = UBeyondLootSubsystem::Get(Angel);
		if (!Angel || !PC || !Loot)
		{
			return true;
		}
		T.TestTrue(TEXT("The boss-ranked kill dropped 3 items on the ground"), Loot->GetDropCount() >= FMath::RoundToInt(State->Numbers.FindRef(TEXT("Drops"))) + 3);

		// A drop at Angel's feet: F picks it up
		const FBeyondItemInstance Item = UBeyondItemLibrary::MakeItem(State->Plain.Get(), EBeyondItemTier::Epic, 3);
		const TArray<ABeyondLootDrop*> Spawned = Loot->SpawnDrops({ Item }, Angel->GetActorLocation());
		if (!T.TestEqual(TEXT("A drop spawns"), Spawned.Num(), 1))
		{
			return true;
		}
		T.TestTrue(TEXT("The drop is the pickup target"), PC->FindPickupTarget() == Spawned[0]);
		T.TestFalse(TEXT("The drop has a label"), Spawned[0]->GetLabel().IsEmpty());

		UBeyondProgressWidget* Progress = Cast<UBeyondProgressWidget>(PC->GetProgressWidget());
		const int32 Toasts = Progress ? Progress->GetToastCount() : 0;
		T.TestTrue(TEXT("F picks it up"), PC->PickUpNearestLoot());
		FBeyondItemInstance Found;
		T.TestTrue(TEXT("It's in the bag"), PC->InventoryComponent->FindItem(Item.Id, Found));
		if (Progress)
		{
			T.TestTrue(TEXT("A pickup toast shows"), Progress->GetToastCount() > Toasts);
		}

		// A full bag refuses (and says so)
		const int32 Capacity = PC->InventoryComponent->Capacity;
		PC->InventoryComponent->Capacity = PC->InventoryComponent->GetItemCount();
		const TArray<ABeyondLootDrop*> Another = Loot->SpawnDrops({ UBeyondItemLibrary::MakeItem(State->Plain.Get(), EBeyondItemTier::Common, 1) }, Angel->GetActorLocation());
		T.TestEqual(TEXT("Another drop spawns"), Another.Num(), 1);
		T.TestFalse(TEXT("A full bag can't take more"), PC->PickUpNearestLoot());
		if (Progress)
		{
			T.TestFalse(TEXT("The HUD says the bag is full"), Progress->GetNotice().IsEmpty());
		}
		PC->InventoryComponent->Capacity = Capacity;
		T.TestTrue(TEXT("With room again, it picks up"), PC->PickUpNearestLoot());
		State->PickupDrop = Spawned[0];
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));

	// The I screen: tabs, equipping through it, refusals, discarding, switching to the skill tree
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondPlayerController* PC = State->PC.Get();
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();
		if (!PC || !JiWoong)
		{
			return true;
		}
		T.TestFalse(TEXT("The picked-up drop is gone"), State->PickupDrop.IsValid());

		PC->OpenInventory();
		UBeyondInventoryWidget* Screen = Cast<UBeyondInventoryWidget>(PC->GetInventoryWidget());
		if (!T.TestNotNull(TEXT("Equipment & inventory screen"), Screen))
		{
			return true;
		}
		T.TestTrue(TEXT("The screen is open"), PC->IsInventoryOpen());
		T.TestTrue(TEXT("The game is paused while it's open"), PC->GetWorld()->IsPaused());
		T.TestEqual(TEXT("One tab per demigod"), Screen->GetTabCount(), 2);
		T.TestTrue(TEXT("It opens on the leader"), Screen->GetShownCharacter() == PC->GetPawn());
		Screen->SelectTab(Screen->FindTabFor(JiWoong));
		T.TestTrue(TEXT("Ji-Woong's tab"), Screen->GetShownCharacter() == JiWoong);

		const UBeyondItemDatabase* Database = UBeyondItemLibrary::GetItemDatabase();
		const UBeyondItemDefinition* Sword = FindDatabaseItem(Database, [](const UBeyondItemDefinition& Item)
		{
			return Item.Slot == EBeyondItemSlot::Weapon && Item.WeaponTag.ToString().Contains(TEXT("Sword"));
		});
		const UBeyondItemDefinition* Staff = FindDatabaseItem(Database, [](const UBeyondItemDefinition& Item)
		{
			return Item.Slot == EBeyondItemSlot::Weapon && Item.WeaponTag.ToString().Contains(TEXT("Staff"));
		});
		UBeyondInventoryComponent* Bag = PC->InventoryComponent;
		if (Sword && Staff)
		{
			const FBeyondItemInstance Blade = UBeyondItemLibrary::MakeItem(Sword, EBeyondItemTier::Epic, 2);
			const FBeyondItemInstance Rod = UBeyondItemLibrary::MakeItem(Staff, EBeyondItemTier::Epic, 2);
			Bag->AddItem(Blade);
			Bag->AddItem(Rod);
			T.TestTrue(TEXT("The bag lists the new items"), Screen->GetSortedBagItems().ContainsByPredicate([&Blade](const FBeyondItemInstance& Item) { return Item.Id == Blade.Id; }));

			T.TestTrue(TEXT("Screen: equip the sword on Ji-Woong"), Screen->EquipBagItem(Blade.Id));
			FBeyondItemInstance Worn;
			T.TestTrue(TEXT("Ji-Woong holds it"), JiWoong->GetEquipmentComponent()->GetEquipped(EBeyondItemSlot::Weapon, Worn) && Worn.Id == Blade.Id);
			T.TestFalse(TEXT("Screen: a staff is refused on Ji-Woong"), Screen->EquipBagItem(Rod.Id));
			T.TestFalse(TEXT("...with a reason"), Screen->GetMessage().IsEmpty());
			T.TestTrue(TEXT("Screen: take the sword off"), Screen->UnequipSlot(EBeyondItemSlot::Weapon));
			FBeyondItemInstance Found;
			T.TestTrue(TEXT("The sword is back in the bag"), Bag->FindItem(Blade.Id, Found));
			T.TestTrue(TEXT("Screen: discard the staff"), Screen->DiscardBagItem(Rod.Id));
			T.TestFalse(TEXT("The staff is gone"), Bag->FindItem(Rod.Id, Found));
		}
		else
		{
			T.AddError(TEXT("migrate_pass9: no sword / staff in the item database"));
		}

		// Sorted by slot, best tier first
		const TArray<FBeyondItemInstance> Sorted = Screen->GetSortedBagItems();
		bool bOrdered = true;
		for (int32 Index = 1; Index < Sorted.Num(); ++Index)
		{
			const UBeyondItemDefinition* A = Sorted[Index - 1].GetDefinition();
			const UBeyondItemDefinition* B = Sorted[Index].GetDefinition();
			if (A && B && (A->Slot > B->Slot || (A->Slot == B->Slot && Sorted[Index - 1].Tier < Sorted[Index].Tier)))
			{
				bOrdered = false;
			}
		}
		T.TestTrue(TEXT("The bag is sorted by slot, then tier"), bOrdered);

		// One menu at a time: K from the inventory goes to the skill tree, still paused
		PC->OpenSkillTree();
		T.TestFalse(TEXT("Opening the skill tree closes the inventory"), PC->IsInventoryOpen());
		T.TestTrue(TEXT("The skill tree is open"), PC->IsSkillTreeOpen());
		T.TestTrue(TEXT("Still paused"), PC->GetWorld()->IsPaused());
		PC->CloseSkillTree();
		T.TestFalse(TEXT("Closed"), PC->IsSkillTreeOpen() || PC->IsInventoryOpen());
		T.TestFalse(TEXT("Unpaused"), PC->GetWorld()->IsPaused());
		return true;
	}));

	// Items in the save game (version 3)
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		ABeyondCharacterBase* JiWoong = State->JiWoong.Get();

		const UBeyondItemDefinition* Saved = FindDatabaseItem(UBeyondItemLibrary::GetItemDatabase(), [](const UBeyondItemDefinition& Item) { return Item.Set != nullptr; });
		if (T.TestNotNull(TEXT("migrate_pass9: a set piece to save"), Saved))
		{
			FRandomStream Stream(3);
			const FBeyondItemInstance Item = UBeyondItemLibrary::MakeItemFromStream(Saved, EBeyondItemTier::Legendary, 6, Stream);
			UBeyondSaveGame* Save = Cast<UBeyondSaveGame>(UGameplayStatics::CreateSaveGameObject(UBeyondSaveGame::StaticClass()));
			T.TestEqual(TEXT("Save version 3"), Save->Version, 3);
			Save->Inventory.Add(Item);
			Save->Members.Add(FName(TEXT("BP_Test_C"))).Equipped.Add(EBeyondItemSlot::Chest, Item);
			Save->bStarterKitGiven = true;
			TArray<uint8> Bytes;
			T.TestTrue(TEXT("Save game serialises"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
			const UBeyondSaveGame* Loaded = Cast<UBeyondSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
			const FBeyondItemInstance* LoadedItem = Loaded && Loaded->Inventory.Num() == 1 ? &Loaded->Inventory[0] : nullptr;
			if (T.TestNotNull(TEXT("The bag round-trips"), LoadedItem))
			{
				T.TestTrue(TEXT("Item id, definition, tier, level and bonus stats survive"),
					LoadedItem->Id == Item.Id && LoadedItem->Definition == Item.Definition && LoadedItem->Tier == Item.Tier
					&& LoadedItem->ItemLevel == Item.ItemLevel && LoadedItem->BonusStats.Num() == Item.BonusStats.Num());
			}
			const FBeyondMemberProgress* Member = Loaded ? Loaded->Members.Find(FName(TEXT("BP_Test_C"))) : nullptr;
			const FBeyondItemInstance* LoadedWorn = Member ? Member->Equipped.Find(EBeyondItemSlot::Chest) : nullptr;
			T.TestTrue(TEXT("Equipped items round-trip"), LoadedWorn && LoadedWorn->Id == Item.Id);
			T.TestTrue(TEXT("The starter kit flag round-trips"), Loaded && Loaded->bStarterKitGiven);
		}

		// Hand Ji-Woong back to his companion AI
		if (AAIController* Holder = State->Holder.Get())
		{
			Holder->UnPossess();
			Holder->Destroy();
		}
		if (AController* Companion = State->CompanionController.Get(); Companion && JiWoong)
		{
			Companion->Possess(JiWoong);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondItemsStep([State]()
	{
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		return true;
	}));
	return true;
}

#endif
