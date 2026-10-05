// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/BeyondFX.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeyondCombatSubsystem.generated.h"

class UAbilitySystemComponent;
class UFXSystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBeyondDamageDealtSignature, AActor*, DamageInstigator, AActor*, Target, float, Damage);

/** What a brand does to the character carrying it (see UBeyondGA_Brand / Sunbrand) */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondBrandSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand", meta = (ClampMin = "0.1"))
	float Duration = 8.0f;

	// All damage the branded character takes is multiplied by this
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand", meta = (ClampMin = "1"))
	float DamageTakenMultiplier = 1.2f;

	// A melee hit from whoever placed the brand detonates it: this much damage to hostiles around the target
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand|Detonation", meta = (ClampMin = "0"))
	float DetonateDamage = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand|Detonation", meta = (ClampMin = "0"))
	float DetonateRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand|Detonation", meta = (Categories = "Event.Hit"))
	FGameplayTag DetonateHitResponse;

	// The brander heals this fraction of the detonation damage dealt (warlock drain)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand|Detonation", meta = (ClampMin = "0"))
	float DrainFraction = 0.5f;

	// Looping effect on the target while branded (attached to its root; use Offset to lift it over the head)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand|FX")
	FBeyondFX MarkFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brand|FX")
	FBeyondFX DetonateFX;
};

/**
 * World-wide combat services:
 * - a damage feed (every Beyond character reports the damage it takes), so systems like the party's Bond meter
 *   don't have to bind to every enemy;
 * - brands: marks that amplify damage taken and detonate on the brander's next melee hit.
 */
UCLASS()
class WORLDBEYOND_API UBeyondCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Damage that got through (blocked / parried hits are not reported)
	UPROPERTY(BlueprintAssignable, Category = "Beyond|Combat")
	FBeyondDamageDealtSignature OnDamageDealt;

	static UBeyondCombatSubsystem* Get(const UObject* WorldContext);

	// Brand Target for Settings.Duration (replaces a brand it already carries)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Brand")
	bool ApplyBrand(AActor* Source, AActor* Target, const FBeyondBrandSettings& Settings);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat|Brand")
	bool IsBranded(const AActor* Target) const;

	// Called by UCharacterAttributeSet before damage lands: amplifies it and triggers detonations
	float ModifyIncomingDamage(UAbilitySystemComponent& TargetASC, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags, float Damage);

private:
	struct FBrandState
	{
		TWeakObjectPtr<AActor> Source;
		TWeakObjectPtr<AActor> Target;
		FActiveGameplayEffectHandle Effect;
		FBeyondBrandSettings Settings;
		TWeakObjectPtr<UFXSystemComponent> Mark;
		bool bDetonating = false;
	};

	// bRemoveEffect is false when the effect is already on its way out (expired)
	void ClearBrand(UAbilitySystemComponent* TargetASC, FActiveGameplayEffectHandle OnlyIfEffect = FActiveGameplayEffectHandle(), bool bRemoveEffect = true);
	void Detonate(TWeakObjectPtr<UAbilitySystemComponent> TargetASC);
	static void CleanUp(UAbilitySystemComponent* TargetASC, FBrandState& State, bool bRemoveEffect);

	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FBrandState> Brands;
};
