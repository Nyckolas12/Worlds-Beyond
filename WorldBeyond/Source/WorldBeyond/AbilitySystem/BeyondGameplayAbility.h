// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BeyondGameplayAbility.generated.h"

class ABeyondCharacterBase;

UENUM(BlueprintType)
enum class EBeyondAITargeting : uint8
{
	// Used on the current hostile target; range is measured to it
	Enemy,
	// Used on self / around self (heals, buffs, auras); range is ignored
	Self
};

/**
 * Base class for every Worlds Beyond ability (player, companion and enemy).
 * Adds an input slot, passive activation, and hints the AI uses to pick abilities.
 */
UCLASS(Abstract)
class WORLDBEYOND_API UBeyondGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGameplayAbility();

	// Default input slot; an entry in UBeyondAbilitySet can override it
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Ability.Input"))
	FGameplayTag InputTag;

	// Activate as soon as the ability is granted (passives, auras)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activation")
	bool bActivateOnGranted = false;

	// AI (companion / enemies) may pick this ability
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	bool bAIUsable = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable"))
	EBeyondAITargeting AITargeting = EBeyondAITargeting::Enemy;

	// Distance to the target the ability works in
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0"))
	float AIMinRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0"))
	float AIMaxRange = 300.0f;

	// Higher weight is picked more often when several abilities are ready
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0"))
	float AIWeight = 1.0f;

	// Only pick when the owner or its leader is below this health fraction (1 = always)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0", ClampMax = "1"))
	float AIUseBelowHealthPercent = 1.0f;

	UFUNCTION(BlueprintPure, Category = "Ability")
	ABeyondCharacterBase* GetBeyondCharacter() const;

	// The actor an AI controller is focusing (its target); null for players
	UFUNCTION(BlueprintPure, Category = "Ability|Aim")
	AActor* GetAIFocusTarget() const;

	/**
	 * Where to aim from Origin: at the AI's focus target, or along the player's camera.
	 * Use this in abilities instead of GetPlayerController/camera so the companion and enemies can use them too.
	 */
	UFUNCTION(BlueprintPure, Category = "Ability|Aim")
	FRotator GetAimRotation(FVector Origin) const;

	// Damage through the shared pipeline with this ability's avatar as the instigator
	UFUNCTION(BlueprintCallable, Category = "Ability|Combat")
	bool ApplyDamageToTarget(AActor* Target, float Amount, UPARAM(meta = (Categories = "DamageType")) FGameplayTag DamageType, UPARAM(meta = (Categories = "Event.Hit")) FGameplayTag HitResponse, bool bUnblockable = false);

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
};
