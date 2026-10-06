// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Progression/BeyondSkillTree.h"
#include "Templates/SubclassOf.h"

class UAbilitySystemComponent;

/** Stat bonuses (skill tree, equipment) as one infinite SetByCaller effect per source */
namespace BeyondStats
{
	// The SetByCaller tag UBeyondGE_LevelStats and its children read for a stat
	WORLDBEYOND_API FGameplayTag GetStatTag(EBeyondSkillStat Stat);

	/**
	 * Puts Bonuses on ASC as one EffectClass (a UBeyondGE_LevelStats child) in place of OldHandle and returns the new
	 * handle (invalid when every bonus is 0). The new effect goes on before the old one comes off, so a maximum never
	 * dips and cuts the current value; a higher max health / stamina also raises the current value by the difference.
	 */
	WORLDBEYOND_API FActiveGameplayEffectHandle ApplyStatBonuses(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass,
		const TMap<EBeyondSkillStat, float>& Bonuses, FActiveGameplayEffectHandle OldHandle);
}
