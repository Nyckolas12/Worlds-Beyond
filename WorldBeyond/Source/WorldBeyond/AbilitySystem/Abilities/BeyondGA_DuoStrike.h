// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "Characters/BeyondRushComponent.h"
#include "Game/BeyondCombatSubsystem.h"
#include "BeyondGA_DuoStrike.generated.h"

class ABeyondCharacterBase;
class UAnimMontage;
class UBeyondPartyComponent;
class UFXSystemComponent;

/**
 * The demigods' duo super move, "Heaven's Judgment" (Storm Judgment + Heaven & Earth).
 * Needs a full party Bond meter and both demigods alive and close together; works whichever of them the player controls.
 *
 * The move locks onto an enemy when it starts (the crosshair target, else a boss, else the buddy's target, else the
 * nearest one within Target Search Radius of the leader).
 *
 * 1. Heaven   - the Conduit (Angel) channels; flashes of blue/purple lightning strike and lift the enemies around it.
 * 2. Absorb   - the lightning arcs into the Striker (Ji-Woong), whose gold mixes with it.
 *    Approach - charged, the Striker rushes to the enemy (dash animation, blinks the rest if blocked or far).
 * 3. Judgment - the Striker slams down, releasing a shockwave that falls off from the centre; the locked enemy always
 *              takes its full force, so the final blow lands wherever the Striker started.
 *
 * Both demigods are invincible and uninterruptible throughout; the companion AI waits (State.Duo).
 *
 * The other duo powers are this ability with a variant switched on (unlocked in the duo skill tree):
 * - Eclipse Brand: the flashes brand what they strike, the shockwave sets every brand off and lightning chains
 *   between them;
 * - Tempest Aegis: after the shockwave both demigods carry a storm shield that reflects damage.
 * Skill tree ranks raise the ability's level: all its damage x Get Level Damage Scale.
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

	// The enemy the move is aimed at is picked within this distance of the leader
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo", meta = (ClampMin = "0"))
	float TargetSearchRadius = 2500.0f;

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

	// ---- Approach: the charged Striker rushes to the locked enemy

	// Closer than this to the enemy, the Striker slams from where he stands
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2b Approach", meta = (ClampMin = "0"))
	float DashAnimationDistance = 450.0f;

	// Speed, dash animation, trail and blink effects (empty animation / trail: the Striker's own dash ability's)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|2b Approach")
	FBeyondRushSettings Approach;

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

	// ---- Variants (Eclipse Brand, Tempest Aegis); leave them off for Heaven's Judgment

	// Every enemy a Heaven flash strikes gets branded by the Striker
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|Variant")
	bool bBrandStruckEnemies = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|Variant", meta = (EditCondition = "bBrandStruckEnemies"))
	FBeyondBrandSettings StruckBrand;

	// The shockwave sets off every brand in range (after its own damage), and the Conduit's lightning chains through them
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|Variant")
	bool bShockwaveDetonatesBrands = false;

	// Damage of the chain lightning at each branded enemy
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|Variant", meta = (ClampMin = "0", EditCondition = "bShockwaveDetonatesBrands"))
	float ChainDamage = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|Variant", meta = (EditCondition = "bShockwaveDetonatesBrands"))
	FBeyondFX ChainFX;

	// After the shockwave both demigods get this storm shield (Duration 0: none)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duo|Variant")
	FBeyondAegisSettings Aegis;

	// Which party member plays which part; false if the party can't do the move right now (for UI hints)
	UFUNCTION(BlueprintPure, Category = "Duo")
	bool FindDuo(ABeyondCharacterBase*& OutConduit, ABeyondCharacterBase*& OutStriker) const;

	// The enemy the running move is aimed at (none: the Striker slams where he stands)
	UFUNCTION(BlueprintPure, Category = "Duo")
	AActor* GetLockedTarget() const { return LockedTarget.Get(); }

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
	AActor* ChooseTarget(const ABeyondCharacterBase* StrikerCharacter) const;
	void Flash();
	void StartAbsorb();
	void StartApproach();
	void StartJudgment();
	void HandleStrikerEvent(const FGameplayEventData* Payload);
	void Shockwave();
	void DetonateBrandsAround(const FVector& Center);
	void Finish();
	void UnbindStrikerEvent();
	static float PlayMontageOn(ABeyondCharacterBase* Character, UAnimMontage* Montage);
	static void StopMontageOn(ABeyondCharacterBase* Character, UAnimMontage* Montage);
	static void RemoveEffects(TArray<TWeakObjectPtr<UFXSystemComponent>>& Components, bool bLetFade);
	static bool IsBoss(const AActor* Actor);

	TWeakObjectPtr<ABeyondCharacterBase> Conduit;
	TWeakObjectPtr<ABeyondCharacterBase> Striker;
	TWeakObjectPtr<AActor> LockedTarget;
	TArray<TWeakObjectPtr<ABeyondCharacterBase>> TaggedCharacters;
	TArray<TWeakObjectPtr<UFXSystemComponent>> AuraComponents;
	// Effects on the Conduit (the lightning in Angel's hand); they loop, so they must be removed explicitly
	TArray<TWeakObjectPtr<UFXSystemComponent>> ConduitComponents;
	int32 FlashesDone = 0;
	bool bShockwaveDone = false;
	// The controller whose move input the move locked (unlock that one, even if the leader changed)
	TWeakObjectPtr<AController> LockedController;

	FTimerHandle FlashTimer;
	FTimerHandle PhaseTimer;
	FTimerHandle ImpactTimer;
};
