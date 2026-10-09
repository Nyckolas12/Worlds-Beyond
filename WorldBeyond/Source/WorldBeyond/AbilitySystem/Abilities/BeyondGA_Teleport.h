// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondFX.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "BeyondGA_Teleport.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EBeyondTeleportMode : uint8
{
	// Away from the target (a caster getting out of melee)
	AwayFromTarget,
	// Behind the target
	BehindTarget
};

/**
 * Blink (Plan 3B: Veyla): vanish and reappear Distance away from (or behind) the target, on the navmesh. With the AI
 * range set short (AI Max Range) the caster uses it when it's crowded.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGA_Teleport : public UBeyondGameplayAbility
{
	GENERATED_BODY()

public:
	UBeyondGA_Teleport();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	EBeyondTeleportMode Mode = EBeyondTeleportMode::AwayFromTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "100"))
	float Distance = 750.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<UAnimMontage> Montage;

	// Where it vanishes
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	FBeyondFX DepartFX;

	// Where it reappears
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport")
	FBeyondFX ArriveFX;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	FTimerHandle EndTimer;
};
