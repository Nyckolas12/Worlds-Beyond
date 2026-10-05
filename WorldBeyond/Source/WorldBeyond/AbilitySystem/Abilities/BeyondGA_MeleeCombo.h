// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_MeleeCombo.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FBeyondComboStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Event.Hit"))
	FGameplayTag HitResponse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float PlayRate = 1.0f;
};

/**
 * Melee combo: each press during the combo window chains the next step.
 * Hit timing comes from the AN_HitScanStart / AN_HitScanEnd notifies (Event.HitScan.*), the combo window from
 * AN_ContinueComboStart / End (Event.ContinueCombo.*). Montages without those notifies fall back to the
 * HitWindow fractions below. Uses the equipped ABeyondWeapon's blade, or a sphere sweep in front of the character.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_MeleeCombo : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_MeleeCombo();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (TitleProperty = "Montage"))
	TArray<FBeyondComboStep> ComboSteps;

	// Fraction of each montage when hits count, if it has no hit-scan notifies
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Fallback", meta = (ClampMin = "0", ClampMax = "1"))
	float FallbackHitStart = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Fallback", meta = (ClampMin = "0", ClampMax = "1"))
	float FallbackHitEnd = 0.6f;

	// Fraction of each montage after which a press queues the next step, if it has no combo notifies
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Fallback", meta = (ClampMin = "0", ClampMax = "1"))
	float FallbackComboWindowStart = 0.35f;

	/**
	 * For one long montage holding the whole combo (Montage_SwordCombo): when a combo window closes
	 * (AN_ContinueComboEnd) without a press, the montage blends out there instead of playing the next swings.
	 * The AI always presses, so it plays the full combo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	bool bStopIfComboWindowMissed = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0", EditCondition = "bStopIfComboWindowMissed"))
	float MissedWindowBlendOutTime = 0.25f;

	// Sweep used when the character has no ABeyondWeapon equipped
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Unarmed")
	float UnarmedReach = 170.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Unarmed")
	float UnarmedRadius = 60.0f;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	void PlayStep(int32 StepIndex);
	void HandleGameplayEvent(const FGameplayEventData* Payload);
	void StartHitWindow();
	void StopHitWindow();
	void UnarmedSweep();
	void OpenComboWindow();
	void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 StepIndex, int32 Serial);
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 StepIndex, int32 Serial);
	void CloseComboWindow();
	class UAnimInstance* GetAnimInstance() const;

	int32 CurrentStep = 0;
	bool bComboWindowOpen = false;
	bool bNextStepQueued = false;
	bool bGotHitNotify = false;
	bool bGotComboNotify = false;
	bool bHitWindowOpen = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> UnarmedHits;

	FTimerHandle HitStartTimer;
	FTimerHandle HitEndTimer;
	FTimerHandle ComboWindowTimer;
	FTimerHandle UnarmedSweepTimer;
};
