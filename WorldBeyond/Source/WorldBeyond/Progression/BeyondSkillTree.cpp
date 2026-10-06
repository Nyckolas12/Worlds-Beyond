// Fill out your copyright notice in the Description page of Project Settings.

#include "Progression/BeyondSkillTree.h"
#include "AbilitySystem/BeyondGameplayAbility.h"

#define LOCTEXT_NAMESPACE "BeyondSkillTree"

const FBeyondSkillNode* UBeyondSkillTreeAsset::FindNode(FName Id) const
{
	return Nodes.FindByPredicate([Id](const FBeyondSkillNode& Node) { return Node.Id == Id; });
}

FText UBeyondSkillTreeAsset::GetStatName(EBeyondSkillStat Stat)
{
	switch (Stat)
	{
	case EBeyondSkillStat::MaxHealth: return LOCTEXT("MaxHealth", "Max Health");
	case EBeyondSkillStat::MaxStamina: return LOCTEXT("MaxStamina", "Max Stamina");
	case EBeyondSkillStat::Strength: return LOCTEXT("Strength", "Strength");
	case EBeyondSkillStat::Arcana: return LOCTEXT("Arcana", "Arcana");
	case EBeyondSkillStat::Defense: return LOCTEXT("Defense", "Defense");
	}
	return FText::GetEmpty();
}

namespace
{
	FText SkillAbilityName(const TSubclassOf<UGameplayAbility>& Ability)
	{
		if (!Ability)
		{
			return LOCTEXT("NoAbility", "(no ability)");
		}
		FString Name = Ability->GetName();
		Name.RemoveFromEnd(TEXT("_C"));
		Name.RemoveFromStart(TEXT("GA_"));
		// GA_Angel_LightningStrike -> LightningStrike, GA_Duo_HeavensJudgment -> HeavensJudgment
		int32 Underscore = INDEX_NONE;
		if (Name.FindLastChar(TEXT('_'), Underscore))
		{
			Name.RightChopInline(Underscore + 1);
		}
		return FText::FromString(FName::NameToDisplayString(Name, false));
	}
}

TArray<FText> UBeyondSkillTreeAsset::DescribeEffects(const FBeyondSkillNode& Node, int32 Rank)
{
	TArray<FText> Lines;
	const int32 Ranks = FMath::Max(Rank, 1);
	for (const FBeyondSkillEffect& Effect : Node.Effects)
	{
		switch (Effect.Type)
		{
		case EBeyondSkillEffectType::Stat:
			Lines.Add(FText::Format(LOCTEXT("StatLine", "+{0} {1}"), FText::AsNumber(FMath::RoundToInt(Effect.Value * Ranks)), GetStatName(Effect.Stat)));
			break;
		case EBeyondSkillEffectType::AbilityRank:
		{
			const UBeyondGameplayAbility* BeyondCDO = Effect.Ability ? Cast<UBeyondGameplayAbility>(Effect.Ability->GetDefaultObject()) : nullptr;
			const float DamagePercent = (BeyondCDO ? BeyondCDO->DamagePerLevel : 0.15f) * 100.0f * Ranks;
			const float CooldownPercent = (BeyondCDO ? BeyondCDO->CooldownReductionPerLevel : 0.0f) * 100.0f * Ranks;
			Lines.Add(CooldownPercent > 0.0f
				? FText::Format(LOCTEXT("RankLineCooldown", "{0}: +{1}% damage, -{2}% cooldown"), SkillAbilityName(Effect.Ability),
					FText::AsNumber(FMath::RoundToInt(DamagePercent)), FText::AsNumber(FMath::RoundToInt(CooldownPercent)))
				: FText::Format(LOCTEXT("RankLine", "{0}: +{1}% damage"), SkillAbilityName(Effect.Ability), FText::AsNumber(FMath::RoundToInt(DamagePercent))));
			break;
		}
		case EBeyondSkillEffectType::GrantAbility:
			Lines.Add(FText::Format(LOCTEXT("GrantLine", "New ability: {0}"), SkillAbilityName(Effect.Ability)));
			break;
		case EBeyondSkillEffectType::DuoPower:
			Lines.Add(FText::Format(LOCTEXT("DuoLine", "New duo power: {0} (right-click to put it on G)"), SkillAbilityName(Effect.Ability)));
			break;
		case EBeyondSkillEffectType::BondGain:
			Lines.Add(FText::Format(LOCTEXT("BondGainLine", "Bond meter fills {0}% faster"), FText::AsNumber(FMath::RoundToInt(Effect.Value * 100.0f * Ranks))));
			break;
		case EBeyondSkillEffectType::BondEcho:
			Lines.Add(FText::Format(LOCTEXT("BondEchoLine", "After a duo move the Bond meter refills to {0}%"), FText::AsNumber(FMath::RoundToInt(Effect.Value * 100.0f * Ranks))));
			break;
		}
	}
	return Lines;
}

TArray<FString> UBeyondSkillTreeAsset::ValidateTree() const
{
	TArray<FString> Problems;
	TSet<FName> Ids;
	for (const FBeyondSkillNode& Node : Nodes)
	{
		if (Node.Id.IsNone())
		{
			Problems.Add(FString::Printf(TEXT("%s: a node has no id"), *GetName()));
		}
		else if (Ids.Contains(Node.Id))
		{
			Problems.Add(FString::Printf(TEXT("%s: duplicate node id %s"), *GetName(), *Node.Id.ToString()));
		}
		Ids.Add(Node.Id);
	}

	for (const FBeyondSkillNode& Node : Nodes)
	{
		for (const FName& Required : Node.Requires)
		{
			if (!Ids.Contains(Required))
			{
				Problems.Add(FString::Printf(TEXT("%s: %s requires missing node %s"), *GetName(), *Node.Id.ToString(), *Required.ToString()));
			}
		}
		for (const FBeyondSkillEffect& Effect : Node.Effects)
		{
			const bool bNeedsAbility = Effect.Type == EBeyondSkillEffectType::AbilityRank || Effect.Type == EBeyondSkillEffectType::GrantAbility
				|| Effect.Type == EBeyondSkillEffectType::DuoPower;
			if (bNeedsAbility && !Effect.Ability)
			{
				Problems.Add(FString::Printf(TEXT("%s: %s has an effect without an ability"), *GetName(), *Node.Id.ToString()));
			}
		}

		// Walk the prerequisites; coming back to the start is a loop nobody could unlock
		TArray<FName> Open = Node.Requires;
		TSet<FName> Seen;
		while (!Open.IsEmpty())
		{
			const FName Current = Open.Pop();
			if (Current == Node.Id)
			{
				Problems.Add(FString::Printf(TEXT("%s: %s requires itself through its prerequisites"), *GetName(), *Node.Id.ToString()));
				break;
			}
			if (Seen.Contains(Current))
			{
				continue;
			}
			Seen.Add(Current);
			if (const FBeyondSkillNode* Required = FindNode(Current))
			{
				Open.Append(Required->Requires);
			}
		}
	}
	return Problems;
}

#undef LOCTEXT_NAMESPACE
