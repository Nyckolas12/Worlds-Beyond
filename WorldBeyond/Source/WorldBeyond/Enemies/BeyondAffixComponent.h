// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/BeyondAbilitySet.h"
#include "BeyondAffixComponent.generated.h"

class ABeyondCharacterBase;
class UBeyondAffixDefinition;
class UFXSystemComponent;

/**
 * Runs an elite's affixes (UBeyondAffixDefinition): auras and tags, extra abilities, on-hit damage over time and
 * lifesteal, damage-taken multipliers and the Warded ward, lightning pulses, and what happens when it dies
 * (lava pools, Brood copies).
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondAffixComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondAffixComponent();

	// Called by the enemy on BeginPlay with its affixes
	void ActivateAffixes(const TArray<TObjectPtr<UBeyondAffixDefinition>>& InAffixes);

	// Damage taken multipliers (and breaking the ward); called from ABeyondEnemyCharacter::ModifyDamageTaken
	float ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const;

	UFUNCTION(BlueprintPure, Category = "Affixes")
	bool HasAffix(UPARAM(meta = (Categories = "Enemy.Affix")) FGameplayTag AffixTag) const;

	// A Warded elite's ward is up (no melee hit broke it lately)
	UFUNCTION(BlueprintPure, Category = "Affixes")
	bool IsWardUp() const;

	const TArray<TObjectPtr<UBeyondAffixDefinition>>& GetAffixes() const { return ActiveAffixes; }

	// Lightning pulses fired so far (tests)
	int32 GetPulseCount() const { return PulseCount; }

	// Force a pulse now (tests)
	void PulseNow();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ABeyondCharacterBase* GetCharacter() const;
	bool IsFromOwner(const AActor* DamageInstigator) const;
	void HandleHitLanded(AActor* DamageInstigator, AActor* Target, float Damage, const FGameplayTagContainer& DamageTags);
	void Pulse(TWeakObjectPtr<UBeyondAffixDefinition> Affix);
	void FirePulse(const UBeyondAffixDefinition* Affix);
	void RestoreWard() const;

	UFUNCTION()
	void HandleOwnerKilled(ABeyondCharacterBase* Character, AActor* Killer);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBeyondAffixDefinition>> ActiveAffixes;

	FDelegateHandle HitLandedHandle;
	TArray<FTimerHandle> PulseTimers;
	TArray<TWeakObjectPtr<UFXSystemComponent>> Auras;
	FBeyondAbilitySetHandles GrantedHandles;
	mutable float WardBrokenUntil = -1.0f;
	mutable FTimerHandle WardTimer;
	int32 PulseCount = 0;
	bool bActivated = false;
};
