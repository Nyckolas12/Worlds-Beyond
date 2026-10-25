// Fill out your copyright notice in the Description page of Project Settings.

#include "Quests/BeyondQuestComponent.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "Characters/BeyondCharacterBase.h"
#include "Dialogue/BeyondDialogueSubsystem.h"
#include "Dialogue/BeyondNPCCharacter.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "Game/BeyondSaveGame.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Items/BeyondEquipmentComponent.h"
#include "Items/BeyondInventoryComponent.h"
#include "Items/BeyondItemLibrary.h"
#include "Misc/AutomationTest.h"
#include "Player/BeyondPartyComponent.h"
#include "World/BeyondWorldSubsystem.h"
#include "WorldBeyond.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBeyondQuestTracking(
		TEXT("Beyond.QuestTracking"),
		-1,
		TEXT("Quests: -1 on, except while automation tests run (default); 0 off; 1 on."));

	UBeyondQuestComponent* FindQuests(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->FindComponentByClass<UBeyondQuestComponent>() : nullptr;
	}

	// Beyond.Quest list | complete <id> | track <id> | reset
	FAutoConsoleCommandWithWorldAndArgs QuestCommand(
		TEXT("Beyond.Quest"),
		TEXT("Quests: Beyond.Quest list | complete <id> | track <id> | reset"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBeyondQuestComponent* Quests = FindQuests(World);
			const UBeyondQuestDatabase* Database = Quests ? Quests->GetDatabase() : nullptr;
			if (!Database)
			{
				UE_LOG(LogBeyond, Warning, TEXT("Beyond.Quest: no quests (needs a game running and the quest database, pass 22)"));
				return;
			}
			const FString Verb = Args.IsEmpty() ? TEXT("list") : Args[0].ToLower();
			const FName QuestId = Args.Num() > 1 ? FName(*Args[1]) : NAME_None;
			if (Verb == TEXT("complete"))
			{
				Quests->CompleteQuest(QuestId);
			}
			else if (Verb == TEXT("track"))
			{
				Quests->SetTrackedQuest(QuestId);
			}
			else if (Verb == TEXT("reset"))
			{
				Quests->ResetQuests();
			}
			const UEnum* States = StaticEnum<EBeyondQuestState>();
			for (const FBeyondQuest& Quest : Database->Quests)
			{
				UE_LOG(LogBeyond, Log, TEXT("Quest %s (%s): %s%s"), *Quest.QuestId.ToString(), *Quest.Title.ToString(),
					*States->GetNameStringByValue(static_cast<int64>(Quests->GetQuestState(Quest.QuestId))), Quests->GetTrackedQuest() == Quest.QuestId ? TEXT(", tracked") : TEXT(""));
			}
		}));
}

UBeyondQuestComponent::UBeyondQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Database = TSoftObjectPtr<UBeyondQuestDatabase>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Quests/DA_QuestDatabase.DA_QuestDatabase")));
}

bool UBeyondQuestComponent::IsQuestTrackingEnabled()
{
	const int32 Mode = CVarBeyondQuestTracking.GetValueOnGameThread();
	return Mode > 0 || (Mode < 0 && !GIsAutomationTesting);
}

UBeyondQuestDatabase* UBeyondQuestComponent::GetDatabase() const
{
	return DatabaseOverride ? DatabaseOverride.Get() : Database.LoadSynchronous();
}

void UBeyondQuestComponent::SetDatabase(UBeyondQuestDatabase* NewDatabase)
{
	DatabaseOverride = NewDatabase;
	Announced.Reset();
	Refresh();
}

const FBeyondQuest* UBeyondQuestComponent::FindQuest(FName QuestId) const
{
	const UBeyondQuestDatabase* Quests = GetDatabase();
	return Quests ? Quests->FindQuest(QuestId) : nullptr;
}

UBeyondPartyComponent* UBeyondQuestComponent::GetParty() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UBeyondPartyComponent>() : nullptr;
}

void UBeyondQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->OnStoryFlagChanged.AddUniqueDynamic(this, &ThisClass::HandleStoryFlagChanged);
		Party->OnMemberLevelUp.AddUniqueDynamic(this, &ThisClass::HandleMemberLevelUp);
		Party->OnBondLevelChanged.AddUniqueDynamic(this, &ThisClass::HandleBondLevelChanged);
		Party->OnPartyFormed.AddUniqueDynamic(this, &ThisClass::HandlePartyFormed);
	}
	if (UBeyondInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UBeyondInventoryComponent>())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnCharacterKilled.AddUniqueDynamic(this, &ThisClass::HandleCharacterKilled);
	}
	if (UBeyondWorldSubsystem* World = UBeyondWorldSubsystem::Get(this))
	{
		World->OnDiscovered.AddUniqueDynamic(this, &ThisClass::HandleDiscovered);
		World->OnWaystoneAttuned.AddUniqueDynamic(this, &ThisClass::HandleWaystoneAttuned);
	}
	if (UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this))
	{
		Dialogue->OnConversationStarted.AddUniqueDynamic(this, &ThisClass::HandleConversationStarted);
	}
}

void UBeyondQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this))
	{
		Combat->OnCharacterKilled.RemoveAll(this);
	}
	if (UBeyondWorldSubsystem* World = UBeyondWorldSubsystem::Get(this))
	{
		World->OnDiscovered.RemoveAll(this);
		World->OnWaystoneAttuned.RemoveAll(this);
	}
	if (UBeyondDialogueSubsystem* Dialogue = UBeyondDialogueSubsystem::Get(this))
	{
		Dialogue->OnConversationStarted.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

//~ State

bool UBeyondQuestComponent::AreRequirementsMet(const FBeyondQuest& Quest) const
{
	const UBeyondPartyComponent* Party = GetParty();
	if (!Party || Party->GetPartyLevel() < Quest.RequiredLevel || (!Quest.RequiredFlag.IsNone() && !Party->HasStoryFlag(Quest.RequiredFlag)))
	{
		return false;
	}
	for (const FName& Required : Quest.Requires)
	{
		if (GetQuestState(Required) != EBeyondQuestState::Completed)
		{
			return false;
		}
	}
	return true;
}

EBeyondQuestState UBeyondQuestComponent::GetQuestState(FName QuestId) const
{
	if (const FBeyondQuestProgress* Found = Progress.Find(QuestId); Found && Found->bCompleted)
	{
		return EBeyondQuestState::Completed;
	}
	const FBeyondQuest* Quest = FindQuest(QuestId);
	return Quest && AreRequirementsMet(*Quest) ? EBeyondQuestState::Active : EBeyondQuestState::Locked;
}

int32 UBeyondQuestComponent::MeasureObjective(const FBeyondQuest& Quest, int32 ObjectiveIndex) const
{
	const UBeyondPartyComponent* Party = GetParty();
	if (!Party || !Quest.Objectives.IsValidIndex(ObjectiveIndex))
	{
		return 0;
	}
	const FBeyondQuestObjective& Objective = Quest.Objectives[ObjectiveIndex];
	const FBeyondQuestProgress* Saved = Progress.Find(Quest.QuestId);
	const int32 Counted = Saved && Saved->Counts.IsValidIndex(ObjectiveIndex) ? Saved->Counts[ObjectiveIndex] : 0;
	int32 Amount = 0;
	switch (Objective.Type)
	{
	case EBeyondObjectiveType::Flag:
		Amount = Party->HasStoryFlag(Objective.Target) ? Objective.Count : 0;
		break;
	case EBeyondObjectiveType::KillEnemies:
		Amount = Counted;
		break;
	case EBeyondObjectiveType::CollectItem:
	{
		auto Matches = [&Objective](const FBeyondItemInstance& Item) { return FName(*Item.Definition.GetAssetName()) == Objective.Target; };
		if (const UBeyondInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UBeyondInventoryComponent>())
		{
			Amount += Inventory->GetItems().FilterByPredicate(Matches).Num();
		}
		for (const ABeyondCharacterBase* Member : Party->GetMembers())
		{
			if (const UBeyondEquipmentComponent* Equipment = IsValid(Member) ? Member->GetEquipmentComponent() : nullptr)
			{
				for (const TPair<EBeyondItemSlot, FBeyondItemInstance>& Pair : Equipment->GetEquippedItems())
				{
					Amount += Matches(Pair.Value) ? 1 : 0;
				}
			}
		}
		break;
	}
	case EBeyondObjectiveType::ReachLevel:
		Amount = Party->GetPartyLevel();
		break;
	case EBeyondObjectiveType::AttuneWaystones:
		Amount = Party->GetAttunedWaystoneCount();
		break;
	case EBeyondObjectiveType::ReachBondLevel:
		Amount = Party->GetBondLevel();
		break;
	case EBeyondObjectiveType::CompleteQuest:
		Amount = GetQuestState(Objective.Target) == EBeyondQuestState::Completed ? Objective.Count : 0;
		break;
	case EBeyondObjectiveType::TalkTo:
	{
		// A conversation counted it, or the dialogue's Talked.<NPC>.<row> flags say they already spoke
		Amount = Counted;
		const FString Prefix = FString::Printf(TEXT("Talked.%s."), *Objective.Target.ToString());
		for (const FName& Flag : Party->GetStoryFlags())
		{
			if (Flag.ToString().StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				Amount = Objective.Count;
				break;
			}
		}
		break;
	}
	}
	return FMath::Clamp(Amount, 0, Objective.Count);
}

int32 UBeyondQuestComponent::GetObjectiveProgress(FName QuestId, int32 ObjectiveIndex) const
{
	if (const FBeyondQuest* Quest = FindQuest(QuestId))
	{
		const bool bDone = GetQuestState(QuestId) == EBeyondQuestState::Completed;
		return bDone && Quest->Objectives.IsValidIndex(ObjectiveIndex) ? Quest->Objectives[ObjectiveIndex].Count : MeasureObjective(*Quest, ObjectiveIndex);
	}
	return 0;
}

bool UBeyondQuestComponent::IsObjectiveDone(FName QuestId, int32 ObjectiveIndex) const
{
	const FBeyondQuest* Quest = FindQuest(QuestId);
	return Quest && Quest->Objectives.IsValidIndex(ObjectiveIndex) && GetObjectiveProgress(QuestId, ObjectiveIndex) >= Quest->Objectives[ObjectiveIndex].Count;
}

int32 UBeyondQuestComponent::GetCurrentObjective(FName QuestId) const
{
	const FBeyondQuest* Quest = FindQuest(QuestId);
	if (!Quest || GetQuestState(QuestId) != EBeyondQuestState::Active)
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
	{
		if (!IsObjectiveDone(QuestId, Index))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UBeyondQuestComponent::GetQuestCount(EBeyondQuestKind Kind) const
{
	const UBeyondQuestDatabase* Quests = GetDatabase();
	return Quests ? Quests->Quests.FilterByPredicate([Kind](const FBeyondQuest& Quest) { return Quest.Kind == Kind; }).Num() : 0;
}

int32 UBeyondQuestComponent::GetCompletedCount(EBeyondQuestKind Kind) const
{
	int32 Count = 0;
	if (const UBeyondQuestDatabase* Quests = GetDatabase())
	{
		for (const FBeyondQuest& Quest : Quests->Quests)
		{
			Count += Quest.Kind == Kind && GetQuestState(Quest.QuestId) == EBeyondQuestState::Completed ? 1 : 0;
		}
	}
	return Count;
}

bool UBeyondQuestComponent::SetTrackedQuest(FName QuestId)
{
	if (!QuestId.IsNone() && GetQuestState(QuestId) != EBeyondQuestState::Active)
	{
		return false;
	}
	TrackedQuest = QuestId;
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->QueueSave();
	}
	return true;
}

FName UBeyondQuestComponent::GetFocusQuest() const
{
	if (!TrackedQuest.IsNone() && GetQuestState(TrackedQuest) == EBeyondQuestState::Active)
	{
		return TrackedQuest;
	}
	const UBeyondQuestDatabase* Quests = GetDatabase();
	FName AnyActive;
	for (const FBeyondQuest& Quest : Quests ? Quests->Quests : TArray<FBeyondQuest>())
	{
		if (GetQuestState(Quest.QuestId) == EBeyondQuestState::Active)
		{
			if (Quest.Kind == EBeyondQuestKind::Main)
			{
				return Quest.QuestId;
			}
			AnyActive = AnyActive.IsNone() ? Quest.QuestId : AnyActive;
		}
	}
	return AnyActive;
}

//~ Checking and completing

FBeyondQuestProgress& UBeyondQuestComponent::FindOrAddProgress(const FBeyondQuest& Quest)
{
	FBeyondQuestProgress& Found = Progress.FindOrAdd(Quest.QuestId);
	Found.QuestId = Quest.QuestId;
	if (Found.Counts.Num() < Quest.Objectives.Num())
	{
		Found.Counts.SetNumZeroed(Quest.Objectives.Num());
	}
	return Found;
}

void UBeyondQuestComponent::Refresh()
{
	const UBeyondQuestDatabase* Quests = GetDatabase();
	UBeyondPartyComponent* Party = GetParty();
	if (!Quests || !Party || !Party->IsPartyFormed() || bRefreshing || !IsQuestTrackingEnabled())
	{
		return;
	}
	TGuardValue<bool> Guard(bRefreshing, true);
	const bool bCatchUp = bCatchingUp;
	bCatchingUp = false;

	// Completing one quest can start (and already finish) the next, so go round until nothing changes
	bool bAnyCompleted = false;
	for (bool bChanged = true; bChanged;)
	{
		bChanged = false;
		for (const FBeyondQuest& Quest : Quests->Quests)
		{
			if (Quest.Objectives.IsEmpty() || GetQuestState(Quest.QuestId) != EBeyondQuestState::Active)
			{
				continue;
			}
			FindOrAddProgress(Quest);
			if (!Announced.Contains(Quest.QuestId))
			{
				Announced.Add(Quest.QuestId);
				if (!bCatchUp)
				{
					UE_LOG(LogBeyond, Log, TEXT("Quest started: %s"), *Quest.QuestId.ToString());
					OnQuestStarted.Broadcast(Quest.QuestId);
				}
			}
			bool bDone = true;
			for (int32 Index = 0; Index < Quest.Objectives.Num() && bDone; ++Index)
			{
				bDone = MeasureObjective(Quest, Index) >= Quest.Objectives[Index].Count;
			}
			if (bDone)
			{
				FinishQuest(Quest, !bCatchUp);
				bChanged = true;
				bAnyCompleted = true;
			}
		}
	}
	if (!TrackedQuest.IsNone() && GetQuestState(TrackedQuest) != EBeyondQuestState::Active)
	{
		TrackedQuest = NAME_None;
	}
	if (bAnyCompleted)
	{
		Party->QueueSave();
	}
}

void UBeyondQuestComponent::FinishQuest(const FBeyondQuest& Quest, bool bGrantRewards)
{
	FBeyondQuestProgress& Done = FindOrAddProgress(Quest);
	Done.bCompleted = true;
	Announced.Add(Quest.QuestId);
	UE_LOG(LogBeyond, Log, TEXT("Quest completed: %s%s"), *Quest.QuestId.ToString(), bGrantRewards ? TEXT("") : TEXT(" (no rewards)"));
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->SetStoryFlag(Quest.GetCompletionFlag());
	}
	if (bGrantRewards)
	{
		GrantRewards(Quest);
	}
	OnQuestCompleted.Broadcast(Quest.QuestId);
}

void UBeyondQuestComponent::GrantRewards(const FBeyondQuest& Quest)
{
	UBeyondPartyComponent* Party = GetParty();
	UBeyondInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UBeyondInventoryComponent>();
	if (!Party)
	{
		return;
	}
	const FBeyondQuestReward& Reward = Quest.Reward;
	Party->AwardExperience(Reward.Experience);
	Party->AddBondExperience(Reward.BondExperience);
	if (Inventory)
	{
		Inventory->AddCurrencies(Reward.Currencies);
		for (const TSoftObjectPtr<UBeyondItemDefinition>& Entry : Reward.Items)
		{
			if (const UBeyondItemDefinition* Definition = Entry.LoadSynchronous())
			{
				// A full bag still takes a reward
				const FBeyondItemInstance Item = UBeyondItemLibrary::MakeItem(Definition, Reward.ItemTier, Party->GetPartyLevel());
				if (!Inventory->AddItem(Item))
				{
					Inventory->ReturnItem(Item);
				}
			}
		}
	}
}

bool UBeyondQuestComponent::CompleteQuest(FName QuestId, bool bGrantRewards)
{
	const FBeyondQuest* Quest = FindQuest(QuestId);
	if (!Quest || GetQuestState(QuestId) == EBeyondQuestState::Completed)
	{
		return false;
	}
	{
		TGuardValue<bool> Guard(bRefreshing, true);
		FinishQuest(*Quest, bGrantRewards);
	}
	Refresh();
	if (UBeyondPartyComponent* Party = GetParty())
	{
		Party->QueueSave();
	}
	return true;
}

void UBeyondQuestComponent::ResetQuests()
{
	Progress.Reset();
	Announced.Reset();
	TrackedQuest = NAME_None;
	Refresh();
}

void UBeyondQuestComponent::CountObjective(EBeyondObjectiveType Type, FName Target)
{
	const UBeyondQuestDatabase* Quests = GetDatabase();
	if (!Quests || !IsQuestTrackingEnabled())
	{
		return;
	}
	for (const FBeyondQuest& Quest : Quests->Quests)
	{
		if (GetQuestState(Quest.QuestId) != EBeyondQuestState::Active)
		{
			continue;
		}
		bool bCounted = false;
		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			const FBeyondQuestObjective& Objective = Quest.Objectives[Index];
			if (Objective.Type == Type && (Objective.Target.IsNone() || Objective.Target == Target))
			{
				int32& Count = FindOrAddProgress(Quest).Counts[Index];
				if (Count < Objective.Count)
				{
					Count = Type == EBeyondObjectiveType::TalkTo ? Objective.Count : Count + 1;
					bCounted = true;
				}
			}
		}
		if (bCounted)
		{
			OnQuestUpdated.Broadcast(Quest.QuestId);
		}
	}
	Refresh();
}

//~ The game's events

void UBeyondQuestComponent::HandleStoryFlagChanged(FName Flag, bool bSet)
{
	Refresh();
}

void UBeyondQuestComponent::HandleMemberLevelUp(ABeyondCharacterBase* Member, int32 NewLevel)
{
	Refresh();
}

void UBeyondQuestComponent::HandleBondLevelChanged(int32 BondLevel)
{
	Refresh();
}

void UBeyondQuestComponent::HandleInventoryChanged()
{
	Refresh();
}

void UBeyondQuestComponent::HandleDiscovered(FName Id, FText DisplayName)
{
	Refresh();
}

void UBeyondQuestComponent::HandleWaystoneAttuned(ABeyondWaystone* Waystone)
{
	Refresh();
}

void UBeyondQuestComponent::HandlePartyFormed()
{
	Refresh();
}

void UBeyondQuestComponent::HandleCharacterKilled(ABeyondCharacterBase* Victim, AActor* Killer)
{
	const UBeyondPartyComponent* Party = GetParty();
	const ABeyondCharacterBase* Member = Party ? Party->FindMemberFor(Killer) : nullptr;
	const ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(Victim);
	if (Member && Enemy && UBeyondCombatLibrary::AreHostile(Member, Enemy))
	{
		CountObjective(EBeyondObjectiveType::KillEnemies, Enemy->GetDefinition() ? Enemy->GetDefinition()->EnemyId : NAME_None);
	}
}

void UBeyondQuestComponent::HandleConversationStarted(AActor* Speaker, AActor* Partner, FName StartRow)
{
	for (const AActor* Actor : { Speaker, Partner })
	{
		if (const ABeyondNPCCharacter* NPC = Cast<ABeyondNPCCharacter>(Actor); NPC && !NPC->NPCId.IsNone())
		{
			CountObjective(EBeyondObjectiveType::TalkTo, NPC->NPCId);
		}
	}
}

//~ Saving

void UBeyondQuestComponent::WriteSave(UBeyondSaveGame& Save) const
{
	Progress.GenerateValueArray(Save.Quests);
	Save.TrackedQuest = TrackedQuest;
}

void UBeyondQuestComponent::ReadSave(const UBeyondSaveGame& Save)
{
	Progress.Reset();
	for (const FBeyondQuestProgress& Saved : Save.Quests)
	{
		if (!Saved.QuestId.IsNone())
		{
			Progress.Add(Saved.QuestId, Saved);
		}
	}
	TrackedQuest = Save.TrackedQuest;
	// What a saved game already had going isn't announced again
	Announced.Reset();
	for (const TPair<FName, FBeyondQuestProgress>& Pair : Progress)
	{
		Announced.Add(Pair.Key);
	}
	bCatchingUp = Save.Version < 7;
}
