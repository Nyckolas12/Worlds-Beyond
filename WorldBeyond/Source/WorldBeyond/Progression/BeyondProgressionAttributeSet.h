// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterAttributeSet.h"
#include "BeyondProgressionAttributeSet.generated.h"

// Old level, new level
DECLARE_MULTICAST_DELEGATE_TwoParams(FBeyondLevelUpEvent, int32 /*OldLevel*/, int32 /*NewLevel*/);

/**
 * EXP and skill points for the demigods (added at runtime to Player-team characters).
 * EXP arrives through UBeyondGE_GrantExperience (SetByCaller.Experience -> IncomingExperience); crossing a level
 * threshold (UBeyondProgressionSettings) raises the Level attribute in UCharacterAttributeSet and adds skill points.
 */
UCLASS()
class WORLDBEYOND_API UBeyondProgressionAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBeyondProgressionAttributeSet();

	// EXP into the current level
	UPROPERTY(BlueprintReadOnly, Category = "Progression", ReplicatedUsing = OnRep_Experience)
	FGameplayAttributeData Experience;
	ATTRIBUTE_ACCESSORS(UBeyondProgressionAttributeSet, Experience)

	// Unspent skill points (spent in the skill tree)
	UPROPERTY(BlueprintReadOnly, Category = "Progression", ReplicatedUsing = OnRep_SkillPoints)
	FGameplayAttributeData SkillPoints;
	ATTRIBUTE_ACCESSORS(UBeyondProgressionAttributeSet, SkillPoints)

	// Meta: EXP to add, turned into Experience / levels in PostGameplayEffectExecute
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingExperience;
	ATTRIBUTE_ACCESSORS(UBeyondProgressionAttributeSet, IncomingExperience)

	// Broadcast after the Level attribute went up (possibly several levels at once)
	FBeyondLevelUpEvent OnLevelUp;

	// Broadcast after EXP was added (amount)
	FSimpleMulticastDelegate OnExperienceChanged;

protected:
	UFUNCTION()
	virtual void OnRep_Experience(const FGameplayAttributeData& OldExperience) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UBeyondProgressionAttributeSet, Experience, OldExperience);
	}
	UFUNCTION()
	virtual void OnRep_SkillPoints(const FGameplayAttributeData& OldSkillPoints) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UBeyondProgressionAttributeSet, SkillPoints, OldSkillPoints);
	}

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
