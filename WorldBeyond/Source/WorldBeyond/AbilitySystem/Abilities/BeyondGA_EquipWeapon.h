// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_EquipWeapon.generated.h"

class UAnimInstance;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FBeyondWeaponLoadout
{
	GENERATED_BODY()

	// Sent as the equip event's target tag (the "1" key sends Weapon.Melee.Sword for Ji-Woong)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Weapon"))
	FGameplayTag WeaponTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AActor> WeaponClass;

	// Socket on the character's animated mesh (Body on MetaHumans)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AttachSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> EquipMontage;

	// Seconds into the equip montage when the weapon appears in the hand
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float AttachTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> UnequipMontage;

	// Seconds into the unequip montage when the weapon is put away
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float DetachTime = 1.0f;

	// Anim Blueprint while holding this weapon (empty: keep the current one)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UAnimInstance> ArmedAnimClass;

	// Max walk speed while holding this weapon (0: keep)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float ArmedWalkSpeed = 0.0f;
};

/**
 * Draws / sheathes weapons with animation. Triggered by Event.Weapon.Equipped with the weapon tag in the
 * event's target tags; sending the tag of the weapon already in hand sheathes it. An EventMagnitude above 0
 * equips instantly (used when a character spawns with its default weapon).
 * The weapon attaches to the animated mesh, so it works on MetaHumans (Body) and regular characters alike.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_EquipWeapon : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_EquipWeapon();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (TitleProperty = "WeaponTag"))
	TArray<FBeyondWeaponLoadout> Loadouts;

	// Anim Blueprint with no weapon in hand (empty: keep)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<UAnimInstance> UnarmedAnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float UnarmedWalkSpeed = 0.0f;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AActor* GetEquippedWeapon() const { return EquippedWeapon.Get(); }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGameplayTag GetEquippedWeaponTag() const { return EquippedTag; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	const FBeyondWeaponLoadout* FindLoadout(const FGameplayTagContainer& Tags) const;
	void BeginEquip(const FBeyondWeaponLoadout& Loadout, bool bInstant);
	void BeginUnequip(bool bInstant);
	void ShowWeapon();
	void PutAwayWeapon();
	void ApplyStance(TSubclassOf<UAnimInstance> AnimClass, float WalkSpeed);
	void FinishAfterMontage(UAnimMontage* Montage, float Duration);
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial);
	void Finish();

	TWeakObjectPtr<AActor> EquippedWeapon;
	FGameplayTag EquippedTag;

	// What to switch to once the montage is over (swapping the anim class mid-montage would cut it off)
	TSubclassOf<UAnimInstance> PendingAnimClass;
	float PendingWalkSpeed = 0.0f;

	FTimerHandle AttachTimer;
	FTimerHandle FinishTimer;
};
