// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_AreaAttack.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EBeyondStrikeAim : uint8
{
	// Centred where the target stands when the attack starts
	AtTarget,
	// Centred on the attacker (slams, novas)
	AtSelf,
	// Starts at the attacker, facing the target (cleaves, charges, beams)
	FromSelfTowardTarget
};

/**
 * Telegraphed enemy attack (Plan 3): when it starts, the spot (and facing) is locked, the attacker stops and turns,
 * the montage plays and one or more ABeyondAreaStrike markers appear and fill over their wind-up before they hit.
 * Several strikes can follow each other (waves on the same spot) or scatter around the target (meteor rain).
 * Hit reactions (stagger and stronger) cancel it, but strikes already on the ground still land.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_AreaAttack : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_AreaAttack();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	EBeyondStrikeAim Aim = EBeyondStrikeAim::AtTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	FBeyondStrikeSettings Strike;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Waves", meta = (ClampMin = "1"))
	int32 Count = 1;

	// Seconds between strikes
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Waves", meta = (ClampMin = "0"))
	float Interval = 0.3f;

	// Strikes after the first land anywhere within this distance of the aim point (0: all on it)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Waves", meta = (ClampMin = "0"))
	float Scatter = 0.0f;

	// Each strike after the first grows by this much (expanding shockwaves)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack|Waves", meta = (ClampMin = "0"))
	float RadiusGrowth = 0.0f;

	// The first marker appears this long after the attack starts
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float FirstStrikeDelay = 0.0f;

	// The attack (and the attacker's commitment) lasts at least this long without a montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float MinDuration = 0.6f;

	// On the attacker when it starts (wind-up glow, roar)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	FBeyondFX CastFX;

	// The markers of the last activation (tests, Blueprint follow-ups)
	UFUNCTION(BlueprintPure, Category = "Attack")
	TArray<ABeyondAreaStrike*> GetLastStrikes() const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	FVector ProjectToGround(const FVector& Location) const;

	TArray<TWeakObjectPtr<ABeyondAreaStrike>> LastStrikes;
	TWeakObjectPtr<UAnimMontage> PlayingMontage;
	FTimerHandle EndTimer;
};
