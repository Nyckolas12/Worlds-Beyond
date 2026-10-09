// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_HitReact.generated.h"

class UAnimMontage;

/**
 * An enemy's hit reaction (Plan 3; C++ enemies have no BPC_DamageSystem doing it). Runs on Event.Hit.*: the montage
 * comes from the enemy definition's Hit Reactions. Stagger, stun and knock-back interrupt whatever the enemy is doing
 * (stun also holds State.Stunned); a light hit only flinches an enemy that isn't attacking, at most once per
 * Light Hit React Cooldown.
 */
UCLASS()
class WORLDBEYOND_API UBeyondGA_HitReact : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_HitReact();

	// Stun lasts at least this long even with a short montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit React", meta = (ClampMin = "0"))
	float MinStunDuration = 1.2f;

	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void CancelOtherAbilities() const;

	float LastLightReactTime = -1000.0f;
	bool bStunned = false;
	FTimerHandle EndTimer;
};
