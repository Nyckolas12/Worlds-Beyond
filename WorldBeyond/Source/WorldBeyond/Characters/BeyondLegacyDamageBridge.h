// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UActorComponent;
class UAbilitySystemComponent;

/**
 * Keeps the old Blueprint BPC_DamageSystem component in sync with GAS, so existing Blueprint logic
 * (hit reactions, death, health bars, blocking) keeps working while GAS owns health.
 * Everything is done by property/delegate name through reflection; nothing here depends on the Blueprint asset.
 */
namespace BeyondLegacyDamage
{
	// The actor's BPC_DamageSystem component, if it has one
	UActorComponent* FindComponent(const AActor* Owner);

	// Legacy MaxHealth configured on the component (0 if none)
	float GetLegacyMaxHealth(const UActorComponent* Component);

	// Copy health values into the component's Health / MaxHealth / IsDead variables
	void SyncHealth(UActorComponent* Component, float Health, float MaxHealth, bool bDead);

	// Tag counts this bridge added, so it only ever removes what it added itself
	struct FStateMirror
	{
		int32 Blocking = 0;
		int32 Invincible = 0;
		int32 Uninterruptible = 0;
	};

	// Mirror the component's IsBlocking / IsInvincible / IsInterruptible flags onto GAS state tags
	void SyncStateTags(const UActorComponent* Component, UAbilitySystemComponent* ASC, FStateMirror& Mirror);

	// Fire the component's OnDamageResponse / OnBlocked / OnDeath event dispatchers
	void BroadcastDamageResponse(UActorComponent* Component, uint8 LegacyDamageResponse, AActor* DamageCauser);
	void BroadcastBlocked(UActorComponent* Component, bool bCanBeParried, AActor* DamageCauser);
	void BroadcastDeath(UActorComponent* Component);

	// Event.Hit.* tag -> E_DamageResponse value (None=0 HitReaction=1 Stagger=2 Stun=3 KnockBack=4)
	uint8 HitResponseToLegacy(const FGameplayTag& HitResponse);
}
