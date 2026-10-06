// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BeyondSkillTree.generated.h"

class UGameplayAbility;
class UTexture2D;

/** Attributes a skill node can raise */
UENUM(BlueprintType)
enum class EBeyondSkillStat : uint8
{
	MaxHealth,
	MaxStamina,
	// Melee damage +1 % per point
	Strength,
	// Ability / spell damage +1 % per point
	Arcana,
	// Damage taken x 100 / (100 + Defense)
	Defense
};

UENUM(BlueprintType)
enum class EBeyondSkillEffectType : uint8
{
	// +Value of Stat per rank
	Stat,
	// +1 level per rank on Ability (and abilities derived from it): more damage, shorter cooldown
	AbilityRank,
	// Grants Ability (on Input Tag, if set) while the node has a rank
	GrantAbility,
	// Duo tree: unlocks Ability as a duo power that can be put on the duo slot (G)
	DuoPower,
	// Duo tree: Bond meter fills Value faster per rank (0.15 = +15 %)
	BondGain,
	// Duo tree: after a duo move the meter refills to Value of its maximum per rank
	BondEcho
};

/** What a tree is paid with */
UENUM(BlueprintType)
enum class EBeyondSkillCurrency : uint8
{
	// The demigod's own points, one per level
	SkillPoints,
	// The party's shared points: every few levels and from main bosses
	BondPoints
};

UENUM(BlueprintType)
enum class EBeyondSkillNodeState : uint8
{
	// Level or prerequisites not met
	Locked,
	// Requirements met, not taken yet
	Available,
	// Taken, more ranks left
	Acquired,
	// Every rank taken
	Maxed
};

USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondSkillEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	EBeyondSkillEffectType Type = EBeyondSkillEffectType::Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (EditCondition = "Type == EBeyondSkillEffectType::Stat", EditConditionHides))
	EBeyondSkillStat Stat = EBeyondSkillStat::MaxHealth;

	// Per rank: stat points, or a fraction for Bond Gain / Bond Echo
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	float Value = 0.0f;

	// Ability Rank / Grant Ability / Duo Power
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UGameplayAbility> Ability;

	// Grant Ability: the key slot (empty: the ability's own Input Tag)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (Categories = "Ability.Input", EditCondition = "Type == EBeyondSkillEffectType::GrantAbility", EditConditionHides))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondSkillNode
{
	GENERATED_BODY()

	// Unique within the tree; saves refer to it, so don't rename nodes players may have taken
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	// Flavour text; the effect lines are written from Effects
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (MultiLine = "true"))
	FText Description;

	// Empty: the ability's icon, or the stat's
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UTexture2D> Icon;

	// Grid cell on the tree screen: X columns from the centre, Y rows from the top
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "1"))
	int32 MaxRank = 1;

	// Points per rank
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0"))
	int32 Cost = 1;

	// Character level (party level for the duo tree) needed for the first rank
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	// Nodes that need at least one rank first
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TArray<FName> Requires;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TArray<FBeyondSkillEffect> Effects;
};

/**
 * One skill tree (Angel's, Ji-Woong's, the duo tree). The look comes from the SkillTreeSystem pack; the rules and
 * effects run in C++ / GAS (UBeyondSkillTreeComponent). Made by migrate_pass8.py, tune it in the editor.
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondSkillTreeAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	FText TreeName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	EBeyondSkillCurrency Currency = EBeyondSkillCurrency::SkillPoints;

	// Lines and available nodes on the tree screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree")
	FLinearColor AccentColor = FLinearColor(0.10f, 0.45f, 1.0f);

	// Duo tree: the duo power every party starts with (Heaven's Judgment)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree", meta = (EditCondition = "Currency == EBeyondSkillCurrency::BondPoints"))
	TSubclassOf<UGameplayAbility> StarterDuoPower;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tree", meta = (TitleProperty = "Id"))
	TArray<FBeyondSkillNode> Nodes;

	const FBeyondSkillNode* FindNode(FName Id) const;

	// Lines for the tooltip: what Rank ranks give ("+4 Arcana", "Arcane Spikes: +15 % damage, -8 % cooldown")
	static TArray<FText> DescribeEffects(const FBeyondSkillNode& Node, int32 Rank);

	// Duplicate ids, missing prerequisites, prerequisite loops (for tests and the migration report); empty when fine
	UFUNCTION(BlueprintPure, Category = "Tree")
	TArray<FString> ValidateTree() const;

	static FText GetStatName(EBeyondSkillStat Stat);
};
