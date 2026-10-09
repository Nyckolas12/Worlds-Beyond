// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondCombatLibrary.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;

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

	/**
	 * Adapter for BPI_Damagable::TakeDamage: pass the S_DamageInfo struct straight in.
	 * Reads Amount, DamageType, DamageResponse, ShouldDamageInvincible, CanBeBlocked, CanBeParried
	 * and ShouldForceInterrupt by name. Returns true if the target took the damage.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Beyond|Combat|Legacy", meta = (CustomStructureParam = "DamageInfo", DefaultToSelf = "Target"))
	static bool ApplyDamageInfo(AActor* Target, AActor* DamageCauser, const int32& DamageInfo);
	DECLARE_FUNCTION(execApplyDamageInfo);

	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat", meta = (DefaultToSelf = "Source"))
	static bool ApplyHeal(AActor* Source, AActor* Target, float Amount);

	/**
	 * Poison / burn: DamagePerSecond every second for Duration seconds (UBeyondGE_DamageOverTime), tagged DamageTag
	 * (a DamageType.Proc.* tag, so the ticks never set off on-hit effects). A new application from the same Source with
	 * the same tag refreshes the old one instead of stacking. TargetFX plays when it wasn't already running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat", meta = (DefaultToSelf = "Source"))
	static bool ApplyDamageOverTime(AActor* Source, AActor* Target, float DamagePerSecond, float Duration,
		UPARAM(meta = (Categories = "DamageType")) FGameplayTag DamageTag, const FBeyondFX& TargetFX);

	/**
	 * Drop-in for the ability system's Get All Abilities in the ability bar: only abilities on a key slot
	 * (Q, E, R, in that order), one per ability, without the duo move (it has its own slot by the Bond meter).
	 */
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|UI")
	static void GetAbilityBarAbilities(UAbilitySystemComponent* AbilitySystem, TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles);

	// Adapter for BPI_Damagable::Heal: heals Target and returns its new health
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Legacy", meta = (DefaultToSelf = "Target"))
	static float HealActor(AActor* Target, float Amount);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static bool IsActorDead(const AActor* Actor);

	// A mini-boss or main boss (rank), or a Blueprint enemy with a boss bar: gets the boss bar, isn't launched around
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static bool IsBoss(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static float GetActorHealth(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static float GetActorMaxHealth(const AActor* Actor);

	// 0..1, or 0 when the actor has no attribute set
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static float GetActorHealthPercent(const AActor* Actor);

	// Team id used by AI (1 = Player, 2 = Enemy, 255 = none)
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static int32 GetActorTeamNumber(const AActor* Actor);

	// True while the actor is running an attack/ability
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static bool IsActorAttacking(const AActor* Actor);

	// Enemy AI attack tokens: limit how many enemies attack Target at once. Non-Beyond actors always succeed.
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|AI", meta = (DefaultToSelf = "Target"))
	static bool ReserveAttackTokens(AActor* Target, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|AI", meta = (DefaultToSelf = "Target"))
	static void ReturnAttackTokens(AActor* Target, int32 Amount);

	// Uses team affiliation (IGenericTeamAgentInterface) on the actors or their controllers
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static bool AreHostile(const AActor* A, const AActor* B);

	// Both actors are on the same team (actors without a team are never friendly)
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat")
	static bool AreFriendly(const AActor* A, const AActor* B);

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

	/**
	 * True while a cutscene holds Actor: a playing level sequence binds it (the intro), or its player controller
	 * is in cinematic mode (sequences that disable movement input, e.g. the boss fight cutscene).
	 */
	UFUNCTION(BlueprintPure, Category = "Beyond|Combat", meta = (DefaultToSelf = "Actor"))
	static bool IsInCutscene(const AActor* Actor);

	// Outgoing damage multiplier from the attacker's stats: 1 + Strength / 100 for melee, 1 + Arcana / 100 for
	// projectiles and explosions (spells, abilities), 1 otherwise
	static float GetDamageScale(const UAbilitySystemComponent* SourceASC, FGameplayTag DamageType);

	// The anim Blueprint has a Slot node with this name (a montage on a slot it doesn't have shows nothing)
	static bool HasAnimSlot(const UAnimInstance* AnimInstance, FName SlotName);

	/**
	 * What the crosshair is on: a trace from the player's camera along the view, ignoring Pawn, its party and what they
	 * hold. The world is traced on Visibility; characters (whose capsules ignore Visibility) are found with a thin sweep
	 * in front of it. False when Pawn has no player controller; OutHit.bBlockingHit tells if it hit anything
	 * (OutHit.TraceEnd is always set).
	 */
	static bool TraceAlongView(const APawn* Pawn, float Range, FHitResult& OutHit);

private:
	static UAbilitySystemComponent* GetASC(const AActor* Actor);
	static bool ApplyDamageInfoImpl(AActor* Target, AActor* DamageCauser, const UStruct* InfoStruct, const void* InfoData);
};
