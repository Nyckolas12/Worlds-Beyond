// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "Game/BeyondCombatSubsystem.h"
#include "BeyondGA_Brand.generated.h"

class UAnimMontage;

/**
 * Brand an enemy (Ji-Woong's Sunbrand): the target takes extra damage, and the caster's next melee hit on it
 * detonates the brand for area damage that heals the caster. Weaves spells into sword play.
 * Players brand the enemy under (or nearest to) the crosshair; the AI brands its focus target.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Brand : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Brand();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand")
	FBeyondBrandSettings Brand;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand|Targeting", meta = (ClampMin = "100"))
	float Range = 1500.0f;

	// Players: enemies within this angle of the crosshair can be picked when the aim misses
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand|Targeting", meta = (ClampMin = "0", ClampMax = "90"))
	float AimAssistAngle = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand")
	TObjectPtr<UAnimMontage> CastMontage;

	// Seconds into the cast to apply the brand when the montage sends no gameplay event
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand", meta = (ClampMin = "0"))
	float FallbackApplyDelay = 0.3f;

	// On the caster's hand when the brand flies
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand|FX")
	FBeyondFX CastFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand|FX")
	FName CastSocket = TEXT("hand_r");

	// Burst on the target when the brand lands
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brand|FX")
	FBeyondFX ApplyFX;

	UFUNCTION(BlueprintPure, Category = "Brand")
	AActor* GetBrandTarget() const { return BrandTarget.Get(); }

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	AActor* FindTarget() const;
	void HandleCastEvent(const FGameplayEventData* Payload);
	void ApplyBrand();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial);
	void UnbindCastEvents();

	TWeakObjectPtr<AActor> BrandTarget;
	bool bApplied = false;
	bool bMontageDone = false;
	FTimerHandle ApplyTimer;
};
