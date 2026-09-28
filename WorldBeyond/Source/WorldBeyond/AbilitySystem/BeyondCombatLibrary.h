// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "BeyondCombatLibrary.generated.h"

class UAbilitySystemComponent;

/**
 * The single combat API for Worlds Beyond. Players, the companion, enemies, projectiles,
 * AOEs and the old BPI_Damagable functions all route through here.
 */
UCLASS()
class WORLDBEYOND_API UBeyondCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Damage Target through the shared GAS pipeline (UBeyondGE_Damage -> IncomingDamage).
	 * Source may have no ability system (environment, traps). Returns false if Target has no ability system.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat", meta = (DefaultToSelf = "Source", AdvancedDisplay = "bUnblockable,bUnparryable,bIgnoreInvincible,bForceInterrupt,Causer"))
	static bool ApplyDamage(AActor* Source, AActor* Target, float Amount,
		UPARAM(meta = (Categories = "DamageType")) FGameplayTag DamageType,
		UPARAM(meta = (Categories = "Event.Hit")) FGameplayTag HitResponse,
		bool bUnblockable = false, AActor* Causer = nullptr,
		bool bUnparryable = false, bool bIgnoreInvincible = false, bool bForceInterrupt = false);

	/**
	 * Adapter for the old S_DamageInfo struct: pass the E_DamageType / E_DamageResponse enums as bytes
	 * (Melee=1 Projectile=2 Explosion=3 Environment=4 / HitReaction=1 Stagger=2 Stun=3 KnockBack=4).
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Legacy", meta = (DefaultToSelf = "Source"))
	static bool ApplyLegacyDamage(AActor* Source, AActor* Target, float Amount, uint8 DamageType, uint8 DamageResponse,
		bool bShouldDamageInvincible, bool bCanBeBlocked, bool bCanBeParried, bool bShouldForceInterrupt);

	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat", meta = (DefaultToSelf = "Source"))
	static bool ApplyHeal(AActor* Source, AActor* Target, float Amount);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static bool IsActorDead(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static float GetActorHealth(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static float GetActorMaxHealth(const AActor* Actor);

	// 0..1, or 0 when the actor has no attribute set
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static float GetActorHealthPercent(const AActor* Actor);

	// Uses team affiliation (IGenericTeamAgentInterface) on the actors or their controllers
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static bool AreHostile(const AActor* A, const AActor* B);

	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat")
	static void SetInvincible(AActor* Actor, bool bInvincible);

	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat")
	static void SetUninterruptible(AActor* Actor, bool bUninterruptible);

	// Add (count 1) or remove a loose state tag such as State.Blocking
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat")
	static void SetStateTag(AActor* Actor, UPARAM(meta = (Categories = "State")) FGameplayTag StateTag, bool bActive);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat|Legacy")
	static FGameplayTag DamageTypeFromLegacy(uint8 DamageType);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat|Legacy")
	static FGameplayTag HitResponseFromLegacy(uint8 DamageResponse);

private:
	static UAbilitySystemComponent* GetASC(const AActor* Actor);
};
