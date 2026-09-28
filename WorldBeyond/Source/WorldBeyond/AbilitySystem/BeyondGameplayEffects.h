// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BeyondGameplayEffects.generated.h"

/**
 * Instant damage. Magnitude comes from SetByCaller.Damage and lands in IncomingDamage,
 * where UCharacterAttributeSet applies blocking, parrying, invincibility and death.
 * Make a Blueprint child to swap the gameplay cue.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBeyondGE_Damage();
};

/** Instant heal. Magnitude comes from SetByCaller.Heal and lands in IncomingHeal. */
UCLASS(Blueprintable)
class WORLDBEYOND_API UBeyondGE_Heal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBeyondGE_Heal();
};
