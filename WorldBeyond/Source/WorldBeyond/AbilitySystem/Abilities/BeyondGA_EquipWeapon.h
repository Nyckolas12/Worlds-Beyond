// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_EquipWeapon.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct FBeyondWeaponLoadout
{
	GENERATED_BODY()

	// Sent as the equip event's target tag (the "1" key sends Weapon.Melee.Sword for Ji-Woong)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Weapon"))
	FGameplayTag WeaponTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AActor> WeaponClass;

	// Socket on the character's animated mesh (Body on MetaHumans) that holds the drawn weapon
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AttachSocket;

	// Where the weapon rests while sheathed: this socket if the mesh has it...
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Holster")
	FName HolsterSocket;

	// ...otherwise this bone, offset by Holster Offset
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Holster")
	FName HolsterBone = TEXT("pelvis");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Holster")
	FTransform HolsterOffset;

	// Played on the ability's Animation Slot (a sequence) or as is (a montage)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> DrawAnimation;

	// Seconds into the draw when the hand takes the weapon from the holster
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0"))
	float GrabTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> SheatheAnimation;

	// Seconds into the sheathe when the weapon goes back into the holster
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0"))
	float ReleaseTime = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.1"))
	float AnimationPlayRate = 1.0f;

	// Idle while this weapon is drawn (written to the anim Blueprint's Idle Variable); empty keeps the current one
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> ArmedIdle;

	// Max walk speed while drawn (0: keep)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float ArmedWalkSpeed = 0.0f;
};

/**
 * Draws and sheathes a weapon that lives on the character: it rests in a holster (Ji-Woong's left hip) and moves
 * to the hand mid-animation. Draw / sheathe play on an upper-body slot so the character keeps walking.
 *
 * Triggered by Event.Weapon.Equipped with the weapon tag in the event's target tags:
 * - no action tag: toggle (the "1" key); Weapon.Action.Draw / Weapon.Action.Sheathe in the instigator tags force one;
 * - Weapon.Action.Instant or an EventMagnitude above 0 skips the animation (spawning with the default weapon).
 *
 * While granted it also watches its owner: a hostile close by draws the weapon, a quiet spell sheathes it again,
 * and an attack montage starting with the weapon sheathed (the sword combo) quick-draws it on the spot.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_EquipWeapon : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_EquipWeapon();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (TitleProperty = "WeaponTag"))
	TArray<FBeyondWeaponLoadout> Loadouts;

	// The default weapon spawns resting in its holster (true) or already in hand
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bSpawnHolstered = true;

	// Slot for the draw / sheathe animations; falls back to DefaultSlot when the anim Blueprint has no such slot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FName AnimationSlot = TEXT("UpperBody");

	// Anim Blueprint variable that holds the idle animation (ABP_JI-Woong: IdleAnimation)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FName IdleVariable = TEXT("IdleAnimation");

	// Idle with the weapon sheathed; empty keeps the anim Blueprint's own
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> UnarmedIdle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float UnarmedWalkSpeed = 0.0f;

	// Draw when a hostile comes this close (0: never)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto", meta = (ClampMin = "0"))
	float AutoDrawRadius = 1000.0f;

	// Sheathe after this many seconds without a hostile within Auto Sheathe Radius or an attack (0: never)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto", meta = (ClampMin = "0"))
	float AutoSheatheDelay = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto", meta = (ClampMin = "0"))
	float AutoSheatheRadius = 1500.0f;

	// Attack montages that need the weapon in hand: starting one with the weapon sheathed draws it instantly
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto")
	TArray<TObjectPtr<UAnimMontage>> DrawOnMontages;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AActor* GetEquippedWeapon() const { return EquippedWeapon.Get(); }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGameplayTag GetEquippedWeaponTag() const { return EquippedTag; }

	// In hand (true) or in its holster / none (false)
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWeaponDrawn() const { return bDrawn && EquippedWeapon.IsValid(); }

	// Ask for a draw / sheathe through the usual gameplay event (Action: Weapon.Action.Draw or .Sheathe)
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void RequestWeaponAction(UPARAM(meta = (Categories = "Weapon.Action")) FGameplayTag Action, bool bInstant = false);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

private:
	const FBeyondWeaponLoadout* FindLoadout(const FGameplayTagContainer& Tags) const;
	const FBeyondWeaponLoadout* GetEquippedLoadout() const;

	bool SpawnWeapon(const FBeyondWeaponLoadout& Loadout);
	void DestroyWeapon();
	void PlaceInHand();
	void PlaceInHolster();
	void ApplyStance(bool bArmed);

	void BeginDraw(bool bInstant);
	void BeginSheathe(bool bInstant);
	float PlayWeaponAnimation(UAnimSequenceBase* Animation, float PlayRate);
	FName ResolveSlot(UAnimInstance* AnimInstance) const;
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial);
	void Finish();

	void AutoTick();
	UAnimInstance* BindAnimInstance();
	UFUNCTION()
	void HandleMontageStarted(UAnimMontage* Montage);
	void QuickDraw();

	TWeakObjectPtr<AActor> EquippedWeapon;
	FGameplayTag EquippedTag;
	bool bDrawn = false;

	// What the running activation is doing: +1 drawing, -1 sheathing
	int32 PendingDirection = 0;
	bool bPendingMoveDone = false;
	TWeakObjectPtr<UAnimMontage> PlayingMontage;

	float LastCombatTime = -1000.0f;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;

	FTimerHandle MoveTimer;
	FTimerHandle FinishTimer;
	FTimerHandle AutoTimer;
};
