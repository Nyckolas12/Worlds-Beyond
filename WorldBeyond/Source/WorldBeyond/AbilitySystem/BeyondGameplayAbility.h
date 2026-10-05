// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BeyondGameplayAbility.generated.h"

class ABeyondCharacterBase;
class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EBeyondAITargeting : uint8
{
	// Used on the current hostile target; range is measured to it
	Enemy,
	// Used on self / around self (heals, buffs, auras); range is ignored
	Self
};

/**
 * Base class for every Worlds Beyond ability (player, companion and enemy).
 * Adds an input slot, passive activation, and hints the AI uses to pick abilities.
 */
UCLASS(Abstract)
class WORLDBEYOND_API UBeyondGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGameplayAbility();

	// Default input slot; an entry in UBeyondAbilitySet can override it
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Ability.Input"))
	FGameplayTag InputTag;

	// Activate as soon as the ability is granted (passives, auras)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activation")
	bool bActivateOnGranted = false;

	// AI (companion / enemies) may pick this ability
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	bool bAIUsable = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable"))
	EBeyondAITargeting AITargeting = EBeyondAITargeting::Enemy;

	// Distance to the target the ability works in
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0"))
	float AIMinRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0"))
	float AIMaxRange = 300.0f;

	// Higher weight is picked more often when several abilities are ready
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0"))
	float AIWeight = 1.0f;

	// Only pick when the owner or its leader is below this health fraction (1 = always)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (EditCondition = "bAIUsable", ClampMin = "0", ClampMax = "1"))
	float AIUseBelowHealthPercent = 1.0f;

	/**
	 * Simple cooldown without making a Gameplay Effect asset: when above 0 and no Cooldown Gameplay Effect Class
	 * is set, committing applies UBeyondGE_Cooldown for this many seconds, granting Cooldown Tags.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldowns", meta = (ClampMin = "0"))
	float CooldownDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldowns", meta = (Categories = "Cooldown"))
	FGameplayTagContainer CooldownTags;

	UFUNCTION(BlueprintPure, Category = "Ability")
	ABeyondCharacterBase* GetBeyondCharacter() const;

	// The actor an AI controller is focusing (its target); null for players
	UFUNCTION(BlueprintPure, Category = "Ability|Aim")
	AActor* GetAIFocusTarget() const;

	/**
	 * Where to aim from Origin: at the AI's focus target, or along the player's camera.
	 * Use this in abilities instead of GetPlayerController/camera so the companion and enemies can use them too.
	 */
	UFUNCTION(BlueprintPure, Category = "Ability|Aim")
	FRotator GetAimRotation(FVector Origin) const;

	// Damage through the shared pipeline with this ability's avatar as the instigator
	UFUNCTION(BlueprintCallable, Category = "Ability|Combat")
	bool ApplyDamageToTarget(AActor* Target, float Amount, UPARAM(meta = (Categories = "DamageType")) FGameplayTag DamageType, UPARAM(meta = (Categories = "Event.Hit")) FGameplayTag HitResponse, bool bUnblockable = false);

	// Living characters hostile to the avatar within Radius of Center
	UFUNCTION(BlueprintCallable, Category = "Ability|Combat")
	TArray<AActor*> FindHostilesInRadius(FVector Center, float Radius) const;

	// The mesh that animates (Body on MetaHumans)
	UFUNCTION(BlueprintPure, Category = "Ability")
	USkeletalMeshComponent* GetAnimatedMesh() const;

	// Plays Montage on the animated mesh; returns its length (0 if it could not play)
	UFUNCTION(BlueprintCallable, Category = "Ability")
	float PlayMontageOnAvatar(UAnimMontage* Montage, float PlayRate = 1.0f);

	//~ UGameplayAbility
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// Dead characters can't use abilities
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData = nullptr) override;

	/**
	 * Bumped on every activation. Montage callbacks carry it, so when an ability is cancelled and cast again right away
	 * (the same instance), the old montage finishing its blend-out isn't mistaken for the new cast being interrupted.
	 */
	int32 ActivationSerial = 0;

	bool IsCurrentActivation(int32 Serial) const { return IsActive() && Serial == ActivationSerial; }

	// Abilities are blocked while their owner performs the duo super move (State.Duo), except the move itself
	bool bUsableDuringDuo = false;
};
