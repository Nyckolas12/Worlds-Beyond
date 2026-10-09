// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Beam.generated.h"

class ABeyondAreaStrike;
class UAnimMontage;
class UFXSystemComponent;

/**
 * A channelled beam (Plan 3B: Veyla's Severing Beam, Kael'thar's Soul Siphon): a lane marker shows where it will
 * fire, then for Duration the beam burns everything hostile in the lane, turning after the target no faster than
 * Turn Rate (outrun it sideways). Drain heals the caster for a share of the damage.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Beam : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Beam();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// Seconds the lane shows before the beam fires
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0"))
	float WindUp = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0.1"))
	float Duration = 3.0f;

	// Degrees per second the beam turns after its target
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0"))
	float TurnRate = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "100"))
	float Length = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "20"))
	float Width = 170.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0"))
	float DamagePerSecond = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (Categories = "DamageType"))
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (Categories = "Event.Hit"))
	FGameplayTag HitResponse;

	// Share of the damage dealt that heals the caster (Soul Siphon)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0"))
	float DrainFraction = 0.0f;

	// On the caster while it fires (Max Lifetime 0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam")
	FBeyondFX BeamFX;

	// Where the beam bites
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam")
	FBeyondFX HitFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Beam")
	FLinearColor LaneColor = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f);

	UFUNCTION(BlueprintPure, Category = "Beam")
	bool IsFiring() const { return bFiring; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void StartFiring();
	void TickBeam();

	static constexpr float BeamTickInterval = 0.1f;

	TWeakObjectPtr<AActor> BeamTarget;
	TWeakObjectPtr<ABeyondAreaStrike> Lane;
	TWeakObjectPtr<UFXSystemComponent> BeamEffect;
	float FiredFor = 0.0f;
	float LastHitFX = 0.0f;
	bool bFiring = false;
	FTimerHandle FireTimer;
	FTimerHandle TickTimer;
};
