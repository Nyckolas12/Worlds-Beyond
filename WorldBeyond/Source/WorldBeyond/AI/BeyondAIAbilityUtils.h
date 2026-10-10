// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"

class AActor;
class UAbilitySystemComponent;
class UBeyondGameplayAbility;

namespace BeyondAI
{
	struct FAbilityChoice
	{
		FGameplayAbilitySpecHandle Handle;
		bool bTargetsSelf = false;
	};

	/**
	 * Picks a ready ability (not on cooldown, affordable, not blocked) using the AI hints on
	 * UBeyondGameplayAbility: range to Target, health threshold, weight. Target may be null
	 * (then only self-targeted abilities qualify). Ally is checked for heal thresholds too.
	 */
	bool SelectAbility(UAbilitySystemComponent* ASC, const AActor* Target, const AActor* Ally, FAbilityChoice& OutChoice);

	// Same, skipping abilities the filter rejects (enemies: attacks whose attack tokens they can't get)
	bool SelectAbility(UAbilitySystemComponent* ASC, const AActor* Target, const AActor* Ally, FAbilityChoice& OutChoice,
		TFunctionRef<bool(const UBeyondGameplayAbility&)> Filter);

	// Largest AIMaxRange among ready, enemy-targeted abilities (how close the AI should get); 0 if none
	float GetPreferredEngageRange(const UAbilitySystemComponent* ASC);

	// True while a non-passive Beyond ability is running (don't interrupt it with movement)
	bool IsUsingAbility(const UAbilitySystemComponent* ASC);
}
