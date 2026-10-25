// Fill out your copyright notice in the Description page of Project Settings.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AI/BeyondEnemyController.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Editor.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "EngineUtils.h"
#include "Game/BeyondSaveGame.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "Player/BeyondPlayerController.h"
#include "Quests/BeyondQuestComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/BeyondQuestLogWidget.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Plan 6, quests, on a test database in MAP_Demo_Main (Play In Editor): a chapter starts at once and completes on its
 * flag, once, with its rewards; that starts a side quest which counts real kills; a quest waiting only for others
 * completes in the same check; flags of conversations and an item in the bag complete theirs; only active quests can be
 * tracked; the quest log lists the chapters and the side quests that started; a save keeps it all without paying twice,
 * and an old save (version 6) catches up without rewards; with tracking off nothing moves. Beyond.QuestTracking is on for
 * the test only (quests stay off in the other tests). Saving is off.
 */
namespace BeyondQuestsTest
{
	struct FState
	{
		FAutomationTestBase* Test = nullptr;
		TStrongObjectPtr<UBeyondQuestDatabase> Database;
		TArray<TWeakObjectPtr<ABeyondEnemyCharacter>> Wolves;
		int32 GoldBeforeKills = 0;
		FString SavedSaveProgress;
		FString SavedTracking;
	};

	UWorld* GetPlayWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	void SetTracking(const TCHAR* Value)
	{
		if (IConsoleVariable* Tracking = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.QuestTracking")))
		{
			Tracking->Set(Value, ECVF_SetByCode);
		}
	}

	FBeyondQuestObjective MakeObjective(EBeyondObjectiveType Type, FName Target, int32 Count = 1)
	{
		FBeyondQuestObjective Objective;
		Objective.Type = Type;
		Objective.Target = Target;
		Objective.Count = Count;
		return Objective;
	}

	UBeyondQuestDatabase* MakeDatabase()
	{
		UBeyondQuestDatabase* Database = NewObject<UBeyondQuestDatabase>(GetTransientPackage());
		auto Add = [Database](FName Id, EBeyondQuestKind Kind, std::initializer_list<FBeyondQuestObjective> Objectives) -> FBeyondQuest&
		{
			FBeyondQuest& Quest = Database->Quests.AddDefaulted_GetRef();
			Quest.QuestId = Id;
			Quest.Kind = Kind;
			Quest.Chapter = Kind == EBeyondQuestKind::Main ? 1 : 0;
			Quest.Title = FText::FromName(Id);
			Quest.Objectives = Objectives;
			return Quest;
		};
		FBeyondQuest& One = Add(TEXT("q_one"), EBeyondQuestKind::Main, { MakeObjective(EBeyondObjectiveType::Flag, TEXT("Test.QuestOne")) });
		One.Reward.Currencies = FBeyondCurrencyBundle(77);
		One.Reward.BondExperience = 3.0f;
		FBeyondQuest& Two = Add(TEXT("q_two"), EBeyondQuestKind::Side, { MakeObjective(EBeyondObjectiveType::KillEnemies, TEXT("forest_wolf"), 2) });
		Two.Requires = { TEXT("q_one") };
		Two.Reward.Currencies = FBeyondCurrencyBundle(11);
		FBeyondQuest& Three = Add(TEXT("q_three"), EBeyondQuestKind::Side,
			{ MakeObjective(EBeyondObjectiveType::ReachLevel, NAME_None, 1), MakeObjective(EBeyondObjectiveType::CompleteQuest, TEXT("q_one")) });
		Three.Requires = { TEXT("q_two") };
		FBeyondQuest& Talk = Add(TEXT("q_talk"), EBeyondQuestKind::Side, { MakeObjective(EBeyondObjectiveType::TalkTo, TEXT("test_npc")) });
		Talk.RequiredFlag = TEXT("Test.TalkOpen");
		FBeyondQuest& Item = Add(TEXT("q_item"), EBeyondQuestKind::Side, { MakeObjective(EBeyondObjectiveType::CollectItem, TEXT("DA_Item_WanderersHelm")) });
		Item.RequiredFlag = TEXT("Test.ItemOpen");
		FBeyondQuest& Last = Add(TEXT("q_last"), EBeyondQuestKind::Side, { MakeObjective(EBeyondObjectiveType::Flag, TEXT("Test.Never")) });
		Last.Requires = { TEXT("q_item") };
		return Database;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBeyondQuestsStep, TFunction<bool()>, Step);
bool FBeyondQuestsStep::Update()
{
	return Step();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBeyondQuestsTest, "WorldsBeyond.Prototype.Quests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBeyondQuestsTest::RunTest(const FString& Parameters)
{
	using namespace BeyondQuestsTest;
	TSharedRef<FState> State = MakeShared<FState>();
	State->Test = this;
	State->Database.Reset(MakeDatabase());
	TestTrue(TEXT("The test quests are valid"), State->Database->ValidateQuests().IsEmpty());
	{
		UBeyondQuestDatabase* Broken = NewObject<UBeyondQuestDatabase>(GetTransientPackage());
		Broken->Quests.AddDefaulted_GetRef().QuestId = TEXT("dup");
		Broken->Quests.AddDefaulted_GetRef().QuestId = TEXT("dup");
		Broken->Quests.Last().Requires = { TEXT("nowhere") };
		TestTrue(TEXT("Duplicates, missing prerequisites and empty quests are reported"), Broken->ValidateQuests().Num() >= 3);
	}
	TestFalse(TEXT("Quests stay off while automation tests run"), UBeyondQuestComponent::IsQuestTrackingEnabled());

	if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
	{
		State->SavedSaveProgress = SaveProgress->GetString();
		SaveProgress->Set(TEXT("0"), ECVF_SetByCode);
	}
	if (IConsoleVariable* Tracking = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.QuestTracking")))
	{
		State->SavedTracking = Tracking->GetString();
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/SICKA_PERSEPOLIS/MAPS/MAP_Demo_Main")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(4.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondQuestsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		UBeyondQuestComponent* Quests = PC ? PC->QuestComponent.Get() : nullptr;
		UBeyondPartyComponent* Party = PC ? PC->PartyComponent.Get() : nullptr;
		UBeyondInventoryComponent* Bag = PC ? PC->InventoryComponent.Get() : nullptr;
		if (!T.TestNotNull(TEXT("The quest component"), Quests) || !Party || !Bag)
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
		SetTracking(TEXT("1"));
		Quests->SetDatabase(State->Database.Get());

		//~ A chapter starts at once and completes on its flag, with its rewards, once
		T.TestEqual(TEXT("Chapter 1 is active"), Quests->GetQuestState(TEXT("q_one")), EBeyondQuestState::Active);
		T.TestEqual(TEXT("The side quest after it is locked"), Quests->GetQuestState(TEXT("q_two")), EBeyondQuestState::Locked);
		T.TestEqual(TEXT("The focus is the story"), Quests->GetFocusQuest(), FName(TEXT("q_one")));
		if (UBeyondQuestLogWidget* Log = Cast<UBeyondQuestLogWidget>(PC->OpenMenuPage(EBeyondMenuPage::Quests)))
		{
			const TArray<FName> Listed = Log->GetListedQuests();
			T.TestTrue(TEXT("The log lists the chapter"), Listed.Contains(TEXT("q_one")));
			T.TestFalse(TEXT("...but not side quests that haven't started"), Listed.Contains(TEXT("q_two")) || Listed.Contains(TEXT("q_last")));
			T.TestEqual(TEXT("...and selects the quest being followed"), Log->GetSelectedQuest(), FName(TEXT("q_one")));
			PC->CloseMenuPage(EBeyondMenuPage::Quests);
		}
		const int32 Gold = Bag->GetCurrency(EBeyondCurrency::Gold);
		const float BondExperience = Party->GetBondExperience();
		Party->SetStoryFlag(TEXT("Test.QuestOne"));
		T.TestEqual(TEXT("Its flag completes it"), Quests->GetQuestState(TEXT("q_one")), EBeyondQuestState::Completed);
		T.TestEqual(TEXT("...paying its gold"), Bag->GetCurrency(EBeyondCurrency::Gold), Gold + 77);
		T.TestEqual(TEXT("...and its Bond EXP"), Party->GetBondExperience(), BondExperience + 3.0f, 0.01f);
		T.TestTrue(TEXT("...and sets Quest.q_one"), Party->HasStoryFlag(TEXT("Quest.q_one")));
		T.TestFalse(TEXT("A completed quest can't complete again"), Quests->CompleteQuest(TEXT("q_one")));
		T.TestEqual(TEXT("...so it paid once"), Bag->GetCurrency(EBeyondCurrency::Gold), Gold + 77);
		T.TestEqual(TEXT("The side quest started"), Quests->GetQuestState(TEXT("q_two")), EBeyondQuestState::Active);
		T.TestFalse(TEXT("A completed quest can't be tracked"), Quests->SetTrackedQuest(TEXT("q_one")));
		T.TestTrue(TEXT("An active one can"), Quests->SetTrackedQuest(TEXT("q_two")) && Quests->GetFocusQuest() == TEXT("q_two"));

		//~ Two real wolf kills
		ABeyondCharacterBase* Angel = Party->GetLeader();
		const UBeyondEnemyRoster* Roster = UBeyondEnemySettings::GetRoster();
		UBeyondEnemyDefinition* WolfDefinition = Roster ? Roster->FindEnemy(TEXT("forest_wolf")) : nullptr;
		UBeyondEnemySubsystem* Enemies = UBeyondEnemySubsystem::Get(World);
		if (!T.TestNotNull(TEXT("forest_wolf in the roster"), WolfDefinition) || !Enemies || !Angel)
		{
			return true;
		}
		State->GoldBeforeKills = Bag->GetCurrency(EBeyondCurrency::Gold);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector Where = Angel->GetActorLocation() + Angel->GetActorForwardVector() * 800.0f + Angel->GetActorRightVector() * (Index * 300.0f);
			if (ABeyondEnemyCharacter* Wolf = Enemies->SpawnEnemy(WolfDefinition, FTransform(Where), FBeyondEnemySpawnParams()))
			{
				State->Wolves.Add(Wolf);
				if (ABeyondEnemyController* AI = Cast<ABeyondEnemyController>(Wolf->GetController()))
				{
					AI->SetBrainEnabled(false);
				}
				UBeyondCombatLibrary::ApplyDamage(Angel, Wolf, 1000000.0f, BeyondTags::DamageType_Melee, FGameplayTag(), true, nullptr, true, true);
			}
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FBeyondQuestsStep([State]()
	{
		FAutomationTestBase& T = *State->Test;
		UWorld* World = GetPlayWorld();
		ABeyondPlayerController* PC = World ? Cast<ABeyondPlayerController>(World->GetFirstPlayerController()) : nullptr;
		UBeyondQuestComponent* Quests = PC ? PC->QuestComponent.Get() : nullptr;
		UBeyondPartyComponent* Party = PC ? PC->PartyComponent.Get() : nullptr;
		UBeyondInventoryComponent* Bag = PC ? PC->InventoryComponent.Get() : nullptr;
		if (!Quests || !Party || !Bag)
		{
			return true;
		}

		//~ Kills, a chain, a conversation, an item
		T.TestEqual(TEXT("Two wolves complete the hunt"), Quests->GetQuestState(TEXT("q_two")), EBeyondQuestState::Completed);
		T.TestTrue(TEXT("...paying its gold (with the kills' own)"), Bag->GetCurrency(EBeyondCurrency::Gold) >= State->GoldBeforeKills + 11);
		T.TestEqual(TEXT("A quest waiting only for others completes in the same check"), Quests->GetQuestState(TEXT("q_three")), EBeyondQuestState::Completed);
		T.TestTrue(TEXT("The finished quest is no longer tracked"), Quests->GetTrackedQuest().IsNone());
		T.TestEqual(TEXT("A quest behind a flag waits"), Quests->GetQuestState(TEXT("q_talk")), EBeyondQuestState::Locked);
		Party->SetStoryFlag(TEXT("Test.TalkOpen"));
		T.TestEqual(TEXT("...and starts on it"), Quests->GetQuestState(TEXT("q_talk")), EBeyondQuestState::Active);
		Party->SetStoryFlag(TEXT("Talked.test_npc.Hello"));
		T.TestEqual(TEXT("Having talked to the NPC completes it"), Quests->GetQuestState(TEXT("q_talk")), EBeyondQuestState::Completed);
		Party->SetStoryFlag(TEXT("Test.ItemOpen"));
		const UBeyondItemDefinition* Helm = LoadObject<UBeyondItemDefinition>(nullptr, TEXT("/Game/WorldsBeyond/Items/Armor/DA_Item_WanderersHelm.DA_Item_WanderersHelm"));
		if (T.TestNotNull(TEXT("The Wanderer's Helm"), Helm))
		{
			T.TestEqual(TEXT("The item quest waits for the helm"), Quests->GetQuestState(TEXT("q_item")), EBeyondQuestState::Active);
			Bag->AddItem(UBeyondItemLibrary::MakeItem(Helm, EBeyondItemTier::Common, 1));
			T.TestEqual(TEXT("...and completes once it is in the bag"), Quests->GetQuestState(TEXT("q_item")), EBeyondQuestState::Completed);
		}
		T.TestEqual(TEXT("Side quests done"), Quests->GetCompletedCount(EBeyondQuestKind::Side), 4);
		if (UBeyondQuestLogWidget* Log = Cast<UBeyondQuestLogWidget>(PC->OpenMenuPage(EBeyondMenuPage::Quests)))
		{
			Log->SelectHeaderTab(2);
			T.TestEqual(TEXT("The Side filter lists the five started side quests"), Log->GetListedQuests().Num(), 5);
			Log->SelectQuest(TEXT("q_last"));
			T.TestTrue(TEXT("T tracks the selected quest"), Log->TrackSelectedQuest() && Quests->GetTrackedQuest() == TEXT("q_last"));
			PC->CloseMenuPage(EBeyondMenuPage::Quests);
		}

		//~ Saving: back as it was, nothing paid twice; an old save catches up without rewards
		UBeyondSaveGame* Save = NewObject<UBeyondSaveGame>();
		Quests->WriteSave(*Save);
		const int32 Gold = Bag->GetCurrency(EBeyondCurrency::Gold);
		UBeyondQuestComponent* Loaded = NewObject<UBeyondQuestComponent>(PC);
		Loaded->ReadSave(*Save);
		Loaded->SetDatabase(State->Database.Get());
		T.TestEqual(TEXT("A save keeps completed quests"), Loaded->GetCompletedCount(EBeyondQuestKind::Side), 4);
		T.TestEqual(TEXT("...the active one"), Loaded->GetQuestState(TEXT("q_last")), EBeyondQuestState::Active);
		T.TestEqual(TEXT("...and the tracked one"), Loaded->GetTrackedQuest(), FName(TEXT("q_last")));
		UBeyondSaveGame* Old = NewObject<UBeyondSaveGame>();
		Old->Version = 6;
		UBeyondQuestComponent* CaughtUp = NewObject<UBeyondQuestComponent>(PC);
		CaughtUp->ReadSave(*Old);
		CaughtUp->SetDatabase(State->Database.Get());
		T.TestEqual(TEXT("An old save catches up on what its flags say"), CaughtUp->GetQuestState(TEXT("q_one")), EBeyondQuestState::Completed);
		T.TestEqual(TEXT("...without paying anything"), Bag->GetCurrency(EBeyondCurrency::Gold), Gold);

		//~ Tracking off: nothing moves
		SetTracking(TEXT("0"));
		Party->SetStoryFlag(TEXT("Test.Never"));
		T.TestEqual(TEXT("With quests off a flag completes nothing"), Quests->GetQuestState(TEXT("q_last")), EBeyondQuestState::Active);

		for (const TCHAR* Flag : { TEXT("Test.QuestOne"), TEXT("Test.TalkOpen"), TEXT("Talked.test_npc.Hello"), TEXT("Test.ItemOpen"), TEXT("Test.Never") })
		{
			Party->SetStoryFlag(Flag, false);
		}
		for (const TWeakObjectPtr<ABeyondEnemyCharacter>& Wolf : State->Wolves)
		{
			if (Wolf.IsValid())
			{
				Wolf->Destroy();
			}
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FBeyondQuestsStep([State]()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			return false;
		}
		if (IConsoleVariable* SaveProgress = IConsoleManager::Get().FindConsoleVariable(TEXT("Beyond.SaveProgress")))
		{
			SaveProgress->Set(State->SavedSaveProgress.IsEmpty() ? TEXT("1") : *State->SavedSaveProgress, ECVF_SetByCode);
		}
		SetTracking(State->SavedTracking.IsEmpty() ? TEXT("-1") : *State->SavedTracking);
		return true;
	}));
	return true;
}

#endif
