// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Summon.generated.h"

class ABeyondEnemyCharacter;
class UAnimMontage;

/**
 * Calls roster enemies in around the caster (Plan 3B: a howl, a war cry). They are summoned (no loot, no EXP) and go
 * for the caster's target. The AI only picks it while fewer than Max Alive of its summons live.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Summon : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Summon();

	// Roster ids, picked at random per summon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	TArray<FName> EnemyIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "1"))
	int32 Count = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "1"))
	int32 MaxAlive = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	float MinDistance = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	float MaxDistance = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	TObjectPtr<UAnimMontage> Montage;

	// Seconds into the montage when they appear
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "0"))
	float SummonDelay = 0.6f;

	// Where each one appears
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	FBeyondFX SummonFX;

	UFUNCTION(BlueprintPure, Category = "Summon")
	int32 GetLiveSummonCount() const;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void SpawnSummons();

	TArray<TWeakObjectPtr<ABeyondEnemyCharacter>> Summons;
	TWeakObjectPtr<AActor> SummonTarget;
	FTimerHandle SummonTimer;
	FTimerHandle EndTimer;
};
