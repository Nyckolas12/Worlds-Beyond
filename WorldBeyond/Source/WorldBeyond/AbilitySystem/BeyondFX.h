// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BeyondFX.generated.h"

class UCameraShakeBase;
class UFXSystemAsset;
class UFXSystemComponent;
class USceneComponent;
class USoundBase;

/**
 * One visual/audio beat of an ability: a Niagara or Cascade system, a sound and an optional camera shake.
 * Abilities expose these so their look can be tuned (or swapped for polished effects) on the ability asset.
 */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondFX
{
	GENERATED_BODY()

	// Niagara system or Cascade particle system
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<UFXSystemAsset> System;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FVector Scale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	FVector Offset = FVector::ZeroVector;

	// Recolour systems that expose a colour parameter (Niagara user parameter / Cascade instance parameter)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (EditCondition = "bOverrideColor"))
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (EditCondition = "bOverrideColor"))
	FName ColorParameter = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FX", meta = (EditCondition = "CameraShake != nullptr"))
	float CameraShakeRadius = 2000.0f;

	bool IsSet() const { return System || Sound || CameraShake; }
};

namespace BeyondFX
{
	// Fire-and-forget at a world location; returns the particle component, if any
	WORLDBEYOND_API UFXSystemComponent* SpawnAtLocation(const UObject* WorldContext, const FBeyondFX& FX, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

	// Attached to a component (follows it); destroy the returned component to stop a looping system early
	WORLDBEYOND_API UFXSystemComponent* SpawnAttached(const FBeyondFX& FX, USceneComponent* AttachTo, FName Socket = NAME_None);
}
