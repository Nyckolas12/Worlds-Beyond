// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "BeyondGA_MeleeCombo.generated.h"

class UAnimInstance;
class UAnimMontage;
class USoundBase;

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
 * Hit timing comes from the AN_HitScanStart / AN_HitScanEnd notifies (Event.HitScan.*). The combo window comes from
 * the montage's own Montage Notify Window (Combo Window Notify Name) when it has one, else from AN_ContinueComboStart /
 * End (Event.ContinueCombo.*). Montages without either fall back to the fractions below.
 * Uses the equipped ABeyondWeapon's blade, or a sphere sweep in front of the character.
 *
 * With Legs Follow Movement, each swing plays on the upper body; while the character stands still a muted
 * full-body copy plays in sync on top, so standing swings keep their footwork and moving ones keep the walk / run.
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

	/**
	 * The combo window stays open this fraction of its own length after AN_ContinueComboEnd
	 * (0.15 = 15 % longer), so a slightly late press still chains. 0 keeps the notify timing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0", ClampMax = "1"))
	float ComboWindowExtension = 0.15f;

	/**
	 * Montage Notify Window marking when a press chains the next swing (Montage_SwordCombo: ResumeComboWindow, the
	 * window the old Blueprint combo used). If the montage has Montage Notify Windows, only this name opens the combo
	 * window and the AN_ContinueCombo events are ignored.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	FName ComboWindowNotifyName = TEXT("ResumeComboWindow");

	// AI users press during every window so their combos flow; off, they swing once, like a player who doesn't press
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combo")
	bool bAIChainsCombo = true;

	// Played once when the combo starts; presses that chain the next swings never repeat it
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Voice")
	TObjectPtr<USoundBase> ComboVoiceLine;

	/**
	 * Swing on the upper body while moving so the legs keep walking / running instead of standing in place.
	 * Needs a slot node named Upper Body Slot in the anim Blueprint (ABP_JI-Woong has one); without it the
	 * montage plays as authored.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Movement")
	bool bLegsFollowMovement = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Movement", meta = (EditCondition = "bLegsFollowMovement"))
	FName UpperBodySlot = TEXT("UpperBody");

	// Ground speed (cm/s) above which the legs follow the movement
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Movement", meta = (ClampMin = "0", EditCondition = "bLegsFollowMovement"))
	float MovingSpeed = 50.0f;

	// Blend between the swing's footwork and the walking legs when starting / stopping mid-combo
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo|Movement", meta = (ClampMin = "0", EditCondition = "bLegsFollowMovement"))
	float LegsBlendTime = 0.2f;

	// How many times this ability has played its voice line (once per combo)
	UFUNCTION(BlueprintPure, Category = "Combo|Voice")
	int32 GetVoiceLinesPlayed() const { return VoiceLinesPlayed; }

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
	// A window's end marker: closes it, a little later with Combo Window Extension
	void EndComboWindow();
	UAnimInstance* GetAnimInstance() const;

	void PlayVoiceLine();
	bool IsMoving() const;
	void UpdateLegs();
	void StopStepMontages(float BlendOutTime);
	bool IsComboMontage(UAnimMontage* Montage) const;

	// Something else (a hit reaction) taking over the character interrupts the combo
	UFUNCTION()
	void HandleAnyMontageStarted(UAnimMontage* Montage);

	// The swing montage's Montage Notify Windows (ResumeComboWindow)
	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
	UFUNCTION()
	void HandleMontageNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
	bool IsComboWindowNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload) const;

	int32 CurrentStep = 0;
	bool bComboWindowOpen = false;
	bool bNextStepQueued = false;
	bool bGotHitNotify = false;
	bool bGotComboNotify = false;
	bool bHitWindowOpen = false;
	// A missed window stopped the swing; notifies still firing while it blends out are ignored
	bool bComboStopped = false;
	// This step's windows come from its Montage Notify Windows, not from the AN_ContinueCombo events
	bool bWindowsFromMontage = false;
	float ComboWindowOpenedTime = 0.0f;
	int32 VoiceLinesPlayed = 0;

	// What the current step plays: the swing (upper-body copy, or the montage itself) and, while standing, the
	// muted full-body copy for the footwork
	TWeakObjectPtr<UAnimMontage> SwingMontage;
	TWeakObjectPtr<UAnimMontage> LegsMontage;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> UnarmedHits;

	FTimerHandle HitStartTimer;
	FTimerHandle HitEndTimer;
	FTimerHandle ComboWindowTimer;
	FTimerHandle ComboCloseTimer;
	FTimerHandle UnarmedSweepTimer;
	FTimerHandle LegsTimer;
};
