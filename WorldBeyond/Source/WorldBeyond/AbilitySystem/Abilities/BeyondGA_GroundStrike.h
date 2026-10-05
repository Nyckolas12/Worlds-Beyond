// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_GroundStrike.generated.h"

class AGameplayAbilityTargetActor;
class UAnimMontage;

/**
 * Strike an area on the ground (Angel's Lightning Strike).
 * Player: hold the key to aim with Target Actor Class (a ground decal), release to cast, right mouse cancels.
 * AI: strikes where its focus target stands.
 * The strike lands on Event.Montage.Trigger / Event.ShootProjectile from the cast montage, or after Fallback Strike Delay,
 * then damages every hostile in Radius through the shared damage pipeline (blocking, parrying, hit reactions).
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_GroundStrike : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_GroundStrike();

	// Aiming reticle for players (e.g. GATargetActor_GroundTrace_Decal). Empty: strike at the crosshair right away.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Targeting", meta = (ClampMin = "100"))
	float MaxTargetRange = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike")
	TObjectPtr<UAnimMontage> CastMontage;

	// Seconds into the cast to strike when the montage sends no gameplay event
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike", meta = (ClampMin = "0"))
	float FallbackStrikeDelay = 0.35f;

	// Between the strike visuals and the damage (lets a falling bolt land)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike", meta = (ClampMin = "0"))
	float ImpactDelay = 0.15f;

	// Existing gameplay cue fired at the strike location (e.g. GameplayCue.LightningBolt)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|FX", meta = (Categories = "GameplayCue"))
	FGameplayTag StrikeCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|FX")
	FBeyondFX StrikeFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike", meta = (ClampMin = "0"))
	float Damage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike", meta = (ClampMin = "1"))
	float Radius = 256.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike", meta = (Categories = "DamageType"))
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike", meta = (Categories = "Event.Hit"))
	FGameplayTag HitResponse;

	UFUNCTION(BlueprintPure, Category = "Strike")
	FVector GetStrikeLocation() const { return StrikeLocation; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Blueprint hook after the damage landed (extra effects, chaining...)
	UFUNCTION(BlueprintImplementableEvent, Category = "Strike")
	void OnStrikeLanded(FVector Location, const TArray<AActor*>& HitActors);

private:
	UFUNCTION()
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetCancelled(const FGameplayAbilityTargetDataHandle& Data);

	bool FindAILocation(FVector& OutLocation) const;
	bool FindCrosshairLocation(FVector& OutLocation) const;
	FVector ProjectToGround(const FVector& Location) const;

	void BeginCast(const FVector& Location);
	void HandleCastEvent(const FGameplayEventData* Payload);
	void Strike();
	void ApplyImpact();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial);
	void UnbindCastEvents();
	void TryEnd();

	FVector StrikeLocation = FVector::ZeroVector;
	bool bStruck = false;
	bool bImpactDone = false;
	bool bMontageDone = false;
	FTimerHandle StrikeTimer;
	FTimerHandle ImpactTimer;
};
