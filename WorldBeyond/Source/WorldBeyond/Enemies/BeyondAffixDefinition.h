// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/BeyondFX.h"
#include "BeyondAffixDefinition.generated.h"

class UBeyondAbilitySet;

UENUM(BlueprintType)
enum class EBeyondAffixDeathAction : uint8
{
	None,
	// Leaves a damaging pool where it died (lava)
	LingeringPool,
	// Splits into smaller copies (Brood)
	Split
};

/**
 * An elite affix (Plan 3): a name prefix, a tint and one or more effects, all handled by UBeyondAffixComponent.
 * Elites roll 1 affix (2 from level 10; Project Settings -> Worlds Beyond Enemies).
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondAffixDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// Used by the console (Beyond.Spawn <id> <level> <affix>)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FName AffixId;

	// Put before the enemy's name ("Molten Raider")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FText Prefix;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix", meta = (MultiLine = "true"))
	FText Description;

	// Granted to the enemy while it lives (Enemy.Affix.*)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix", meta = (Categories = "Enemy.Affix"))
	FGameplayTag AffixTag;

	// Overlay tint (alpha = strength); the first affix's wins
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FLinearColor Tint = FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);

	// Looping aura on the enemy (set its Max Lifetime to 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Affix")
	FBeyondFX AuraFX;

	//~ Stats

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.1"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.1"))
	float ScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BonusStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BonusArcana = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float BonusDefense = 0.0f;

	// Loose tags it carries (State.Uninterruptible for Juggernaut)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (Categories = "State"))
	FGameplayTagContainer GrantedTags;

	// Extra abilities (a lunge for Swift, a spit for Venomous)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UBeyondAbilitySet> GrantedAbilities;

	//~ On hit

	// Its hits add a damage over time (DamageType.Proc.Burn / Poison)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Hit", meta = (ClampMin = "0"))
	float OnHitDamagePerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Hit", meta = (ClampMin = "0"))
	float OnHitDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Hit", meta = (Categories = "DamageType"))
	FGameplayTag OnHitDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Hit")
	FBeyondFX OnHitTargetFX;

	// Share of the damage it deals that heals it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "On Hit", meta = (ClampMin = "0", ClampMax = "2"))
	float Lifesteal = 0.0f;

	//~ Damage taken

	// Damage taken x this per damage type (DamageType.Projectile, DamageType.Melee...; parents match children)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Taken", meta = (Categories = "DamageType"))
	TMap<FGameplayTag, float> DamageTakenMultipliers;

	/**
	 * Warded: the multipliers above only apply while the ward is up. A hit of Ward Break Damage Type breaks it for
	 * Ward Break Duration seconds (Warded's spell resistance: a sword hit opens it to Angel).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Taken")
	bool bWard = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Taken", meta = (EditCondition = "bWard", Categories = "DamageType"))
	FGameplayTag WardBreakDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Taken", meta = (EditCondition = "bWard", ClampMin = "0"))
	float WardBreakDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Taken", meta = (EditCondition = "bWard"))
	FBeyondFX WardBreakFX;

	//~ Pulse

	// A telegraphed ring of damage around it every this many seconds while fighting (0: none)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse", meta = (ClampMin = "0"))
	float PulseInterval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse", meta = (ClampMin = "0"))
	float PulseRadius = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse", meta = (ClampMin = "0"))
	float PulseDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse", meta = (ClampMin = "0"))
	float PulseWindUp = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse", meta = (Categories = "DamageType"))
	FGameplayTag PulseDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pulse")
	FBeyondFX PulseFX;

	//~ Death

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	EBeyondAffixDeathAction DeathAction = EBeyondAffixDeathAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0"))
	float PoolRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0"))
	float PoolDamagePerSecond = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0"))
	float PoolDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	FBeyondFX PoolFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0"))
	int32 SplitCount = 2;

	// Each copy's max health as a share of the original's
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.05", ClampMax = "1"))
	float SplitHealthFraction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.1"))
	float SplitScale = 0.7f;
};
