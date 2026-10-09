// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "Animation/AnimEnums.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Leap.generated.h"

class UAnimMontage;

/**
 * Leap onto the target (Plan 3B: Gorehide's pounce, an avalanche drop): the landing spot is locked and marked when the
 * jump starts, the character flies there on a ballistic arc in Air Time, and the marker's hit lands with it.
 * Root motion is ignored while airborne.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Leap : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Leap();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// Seconds in the air
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0.2"))
	float AirTime = 0.8f;

	// Seconds of wind-up on the ground before take-off (the marker already shows)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0"))
	float TakeOffDelay = 0.25f;

	// Lands this far short of the target (not on top of it)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap", meta = (ClampMin = "0"))
	float LandShortBy = 120.0f;

	// The landing hit (its Wind Up is set to the take-off delay + air time)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Leap")
	FBeyondStrikeSettings Landing;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void TakeOff();
	void Land();

	FVector LandingPoint = FVector::ZeroVector;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bChangedRootMotion = false;
	FTimerHandle TakeOffTimer;
	FTimerHandle LandTimer;
};
