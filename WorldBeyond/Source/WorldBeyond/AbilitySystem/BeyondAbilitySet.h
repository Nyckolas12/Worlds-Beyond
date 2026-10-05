// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "BeyondAbilitySet.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UInputAction;

USTRUCT(BlueprintType)
struct FBeyondAbilitySet_Ability
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Level = 1;

	// Input slot; leave empty to use the ability's own InputTag (UBeyondGameplayAbility)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Ability.Input"))
	FGameplayTag InputTag;

	// Enhanced Input action that presses this slot when a player controls the character
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction;
};

USTRUCT(BlueprintType)
struct FBeyondAbilitySet_Effect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Level = 1.0f;
};

USTRUCT(BlueprintType)
struct FBeyondAbilitySetHandles
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};

/**
 * Everything a character starts with: abilities (with their input slots) and startup effects
 * such as stamina regen. Make one per demigod and per enemy type.
 */
UCLASS(BlueprintType, Const)
class WORLDBEYOND_API UBeyondAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Abilities", meta = (TitleProperty = "Ability"))
	TArray<FBeyondAbilitySet_Ability> Abilities;

	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (TitleProperty = "Effect"))
	TArray<FBeyondAbilitySet_Effect> Effects;

	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, FBeyondAbilitySetHandles* OutHandles = nullptr) const;
};
