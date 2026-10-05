// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_DuoStrike.generated.h"

class ABeyondCharacterBase;
class UAnimMontage;
class UBeyondPartyComponent;
class UFXSystemComponent;

/**
 * The demigods' duo super move, "Heaven's Judgment" (Storm Judgment + Heaven & Earth).
 * Needs a full party Bond meter and both demigods alive and close together; works whichever of them the player controls.
 *
 * 1. Heaven   - the Conduit (Angel) channels; flashes of blue/purple lightning strike and lift nearby enemies.
 * 2. Absorb   - the lightning arcs into the Striker (Ji-Woong), whose gold mixes with it.
 * 3. Judgment - the Striker slams down, releasing a shockwave that falls off from the centre.
 *
 * Both demigods are invincible and uninterruptible throughout; the companion AI waits (State.Duo).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_DuoStrike : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_DuoStrike();

	// The partner must be at least this close
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo", meta = (ClampMin = "0"))
	float PartnerRange = 1500.0f;

	// ---- Heaven: lightning flashes around the Striker

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven")
	TObjectPtr<UAnimMontage> ConduitMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven", meta = (ClampMin = "0"))
	int32 FlashCount = 7;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven", meta = (ClampMin = "0.05"))
	float FlashInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven", meta = (ClampMin = "0"))
	float FlashRadius = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven", meta = (ClampMin = "0"))
	float FlashDamage = 15.0f;

	// Upward launch for each struck enemy (bosses are not launched)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven", meta = (ClampMin = "0"))
	float FlashLaunchSpeed = 500.0f;

	// Alternated per flash: Angel's blue and purple
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|1 Heaven")
	TArray<FBeyondFX> FlashFX;

	// ---- Absorb: the power flows into the Striker

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2 Absorb", meta = (ClampMin = "0"))
	float AbsorbDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2 Absorb")
	TObjectPtr<UAnimMontage> StrikerChargeMontage;

	// On the Conduit's hands as the lightning leaves
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2 Absorb")
	FBeyondFX ConduitReleaseFX;

	// Bolts landing along the path from the Conduit to the Striker
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2 Absorb")
	FBeyondFX ArcFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2 Absorb", meta = (ClampMin = "0"))
	int32 ArcSteps = 4;

	// Attached to the Striker while charged (gold + storm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2 Absorb")
	TArray<FBeyondFX> ChargedAuraFX;

	// ---- Judgment: the shockwave

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment")
	TObjectPtr<UAnimMontage> StrikerSlamMontage;

	// Seconds into the slam montage when the shockwave goes off, if it sends no Event.Montage.Trigger
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (ClampMin = "0"))
	float SlamImpactTime = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (ClampMin = "1"))
	float ShockwaveRadius = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (ClampMin = "0"))
	float ShockwaveDamageCenter = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (ClampMin = "0"))
	float ShockwaveDamageEdge = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (ClampMin = "0"))
	float KnockbackSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (Categories = "Event.Hit"))
	FGameplayTag ShockwaveHitResponse;

	// Layered at the impact point: gold, blue and purple rings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment")
	TArray<FBeyondFX> ShockwaveFX;

	// Time after the shockwave before the demigods can act again
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|3 Judgment", meta = (ClampMin = "0"))
	float RecoveryTime = 0.6f;

	// Which party member plays which part; false if the party can't do the move right now (for UI hints)
	UFUNCTION(BlueprintPure, Category = "Duo")
	bool FindDuo(ABeyondCharacterBase*& OutConduit, ABeyondCharacterBase*& OutStriker) const;

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Blueprint hooks for polish (camera, slow motion, voice lines)
	UFUNCTION(BlueprintImplementableEvent, Category = "Duo")
	void OnPhaseStarted(int32 Phase, ABeyondCharacterBase* ConduitCharacter, ABeyondCharacterBase* StrikerCharacter);

private:
	UBeyondPartyComponent* GetParty() const;
	bool FindDuoFor(const AActor* Avatar, ABeyondCharacterBase*& OutConduit, ABeyondCharacterBase*& OutStriker) const;

	void SetDuoState(ABeyondCharacterBase* Character, bool bActive);
	void Flash();
	void StartAbsorb();
	void StartJudgment();
	void HandleStrikerEvent(const FGameplayEventData* Payload);
	void Shockwave();
	void Finish();
	void UnbindStrikerEvent();
	static float PlayMontageOn(ABeyondCharacterBase* Character, UAnimMontage* Montage);
	static bool IsBoss(const AActor* Actor);

	TWeakObjectPtr<ABeyondCharacterBase> Conduit;
	TWeakObjectPtr<ABeyondCharacterBase> Striker;
	TArray<TWeakObjectPtr<ABeyondCharacterBase>> TaggedCharacters;
	TArray<TWeakObjectPtr<UFXSystemComponent>> AuraComponents;
	int32 FlashesDone = 0;
	bool bShockwaveDone = false;
	bool bIgnoringMoveInput = false;

	FTimerHandle FlashTimer;
	FTimerHandle PhaseTimer;
	FTimerHandle ImpactTimer;
};
