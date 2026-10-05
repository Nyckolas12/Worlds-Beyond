// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "Animation/AnimEnums.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Dash.generated.h"

class UAnimMontage;
class UFXSystemComponent;

/**
 * Dash in the movement direction (players) or at the target (AI), passing through characters with
 * invincibility frames. Enemies the dash crosses are hit after Detonate Delay (Ji-Woong's Gilded Step).
 * Cost and cooldown come from the usual Cost / Cooldown Gameplay Effect Class (e.g. GE_Dash_Cost, GE_Dash_Cooldown).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Dash : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Dash();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0"))
	float DashDistance = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.05"))
	float DashDuration = 0.25f;

	// Horizontal speed kept when the dash ends
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0"))
	float ExitSpeed = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bInvincibleWhileDashing = true;

	// Pass through characters instead of stopping at them
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bPassThroughPawns = true;

	// Its root motion is ignored during the dash (the dash itself moves the character)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// Attached to the character for the dash (a trail)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|FX")
	FBeyondFX TrailFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|FX")
	FName TrailSocket;

	// Damage to each enemy the dash passed through (0 = no damage)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Path Damage", meta = (ClampMin = "0"))
	float PathDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Path Damage", meta = (ClampMin = "1"))
	float PathRadius = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Path Damage", meta = (ClampMin = "0"))
	float DetonateDelay = 0.4f;

	// Melee by default, so the dash sets off the dasher's own brands
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Path Damage", meta = (Categories = "DamageType"))
	FGameplayTag PathDamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Path Damage", meta = (Categories = "Event.Hit"))
	FGameplayTag PathHitResponse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Path Damage")
	FBeyondFX DetonateFX;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	FVector ChooseDirection() const;

	UFUNCTION()
	void HandleDashFinished();

	void Detonate();
	void RestoreCharacter();

	FVector DashStart = FVector::ZeroVector;
	bool bDashing = false;
	bool bAddedInvincible = false;
	TEnumAsByte<ECollisionResponse> PreviousPawnResponse = ECR_Block;
	TEnumAsByte<ERootMotionMode::Type> PreviousRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bChangedRootMotionMode = false;
	TWeakObjectPtr<UFXSystemComponent> Trail;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Crossed;

	// Characters (and what they hold) the capsule ignores while passing through
	TArray<TWeakObjectPtr<AActor>> IgnoredActors;

	FTimerHandle DetonateTimer;
	FTimerHandle SafetyTimer;
};
