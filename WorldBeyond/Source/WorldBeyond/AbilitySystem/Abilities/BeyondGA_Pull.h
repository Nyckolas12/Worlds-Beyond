// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Pull.generated.h"

class UAnimMontage;

/**
 * A tether that yanks (Plan 3B: Veyla's Soul-Link Pull): a line marker toward the target fills, and whoever is still in
 * it when it lands is hit and pulled to Stop Distance in front of the caster. Sidestep the line.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Pull : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Pull();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	TObjectPtr<UAnimMontage> Montage;

	// The line (Shape is forced to Line)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	FBeyondStrikeSettings Line;

	// Pulled victims end up this far from the caster
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull", meta = (ClampMin = "50"))
	float StopDistance = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull", meta = (ClampMin = "0.05"))
	float PullDuration = 0.35f;

	// On whoever gets pulled
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pull")
	FBeyondFX PullFX;

	// Victims of the last pull (tests)
	int32 GetLastPulledCount() const { return LastPulled; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void Yank();

	TWeakObjectPtr<ABeyondAreaStrike> Tether;
	int32 LastPulled = 0;
	FTimerHandle YankTimer;
	FTimerHandle EndTimer;
};
