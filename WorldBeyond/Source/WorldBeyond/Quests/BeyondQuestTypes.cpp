// Fill out your copyright notice in the Description page of Project Settings.

#include "Quests/BeyondQuestTypes.h"

const FBeyondQuest* UBeyondQuestDatabase::FindQuest(FName QuestId) const
{
	return Quests.FindByPredicate([QuestId](const FBeyondQuest& Quest) { return Quest.QuestId == QuestId; });
}

TArray<FString> UBeyondQuestDatabase::ValidateQuests() const
{
	TArray<FString> Problems;
	TSet<FName> Ids;
	for (const FBeyondQuest& Quest : Quests)
	{
		if (Quest.QuestId.IsNone())
		{
			Problems.Add(FString::Printf(TEXT("%s: a quest has no id"), *GetName()));
		}
		else if (Ids.Contains(Quest.QuestId))
		{
			Problems.Add(FString::Printf(TEXT("%s: duplicate quest id %s"), *GetName(), *Quest.QuestId.ToString()));
		}
		Ids.Add(Quest.QuestId);
	}
	for (const FBeyondQuest& Quest : Quests)
	{
		if (Quest.Objectives.IsEmpty())
		{
			Problems.Add(FString::Printf(TEXT("%s: %s has no objectives"), *GetName(), *Quest.QuestId.ToString()));
		}
		for (const FName& Required : Quest.Requires)
		{
			if (!Ids.Contains(Required) || Required == Quest.QuestId)
			{
				Problems.Add(FString::Printf(TEXT("%s: %s requires %s"), *GetName(), *Quest.QuestId.ToString(), Required == Quest.QuestId ? TEXT("itself")
					: *FString::Printf(TEXT("missing quest %s"), *Required.ToString())));
			}
		}
		for (const FBeyondQuestObjective& Objective : Quest.Objectives)
		{
			if (Objective.Type == EBeyondObjectiveType::CompleteQuest && !Ids.Contains(Objective.Target))
			{
				Problems.Add(FString::Printf(TEXT("%s: %s waits for missing quest %s"), *GetName(), *Quest.QuestId.ToString(), *Objective.Target.ToString()));
			}
			const bool bNeedsTarget = Objective.Type == EBeyondObjectiveType::Flag || Objective.Type == EBeyondObjectiveType::CollectItem
				|| Objective.Type == EBeyondObjectiveType::TalkTo;
			if (bNeedsTarget && Objective.Target.IsNone())
			{
				Problems.Add(FString::Printf(TEXT("%s: %s has an objective without a target"), *GetName(), *Quest.QuestId.ToString()));
			}
		}
	}
	return Problems;
}
