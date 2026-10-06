// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/BeyondFX.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "BeyondCombatSubsystem.generated.h"

class ABeyondCharacterBase;
class UAbilitySystemComponent;
class UAnimMontage;
class UFXSystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBeyondDamageDealtSignature, AActor*, DamageInstigator, AActor*, Target, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondCharacterDiedSignature, ABeyondCharacterBase*, Victim, AActor*, Killer);

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

/** A storm shield (Tempest Aegis): the carrier takes less damage and throws part of each hit back at the attacker */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondAegisSettings
{
	GENERATED_BODY()

	// Seconds the shield lasts (0: no shield)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis", meta = (ClampMin = "0"))
	float Duration = 0.0f;

	// Damage taken x (1 - this)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis", meta = (ClampMin = "0", ClampMax = "0.95"))
	float DamageReduction = 0.4f;

	// This fraction of each hit (before the reduction) strikes the attacker as lightning
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis", meta = (ClampMin = "0"))
	float ReflectFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis", meta = (Categories = "Event.Hit"))
	FGameplayTag ReflectHitResponse;

	// Looping effect on the carrier while shielded (set its Max Lifetime to 0; it is removed with the shield)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis")
	FBeyondFX AuraFX;

	// On the attacker when a hit is reflected
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aegis")
	FBeyondFX ReflectFX;
};

/**
 * World-wide combat services:
 * - a damage feed (every Beyond character reports the damage it takes), so systems like the party's Bond meter
 *   don't have to bind to every enemy, and a kill feed (the party's EXP);
 * - brands: marks that amplify damage taken and detonate on the brander's next melee hit;
 * - storm shields (Tempest Aegis): less damage taken, part of it reflected;
 * - montage variants: runtime copies of an attack montage on another slot (the sword combo on the upper body).
 */
UCLASS()
class WORLDBEYOND_API UBeyondCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Damage that got through (blocked / parried hits are not reported)
	UPROPERTY(BlueprintAssignable, Category = "Beyond|Combat")
	FBeyondDamageDealtSignature OnDamageDealt;

	// Every Beyond character's death (Killer: whoever dealt the last damage, may be null)
	UPROPERTY(BlueprintAssignable, Category = "Beyond|Combat")
	FBeyondCharacterDiedSignature OnCharacterKilled;

	static UBeyondCombatSubsystem* Get(const UObject* WorldContext);

	// Brand Target for Settings.Duration (replaces a brand it already carries)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Brand")
	bool ApplyBrand(AActor* Source, AActor* Target, const FBeyondBrandSettings& Settings);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat|Brand")
	bool IsBranded(const AActor* Target) const;

	// Sets a brand off right away (the duo move's shockwave); false if Target carries none
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Brand")
	bool DetonateBrand(AActor* Target);

	// Shield Target for Settings.Duration (replaces a shield it already has)
	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Aegis")
	bool ApplyAegis(AActor* Target, const FBeyondAegisSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Beyond|Combat|Aegis")
	void RemoveAegis(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Beyond|Combat|Aegis")
	bool HasAegis(const AActor* Target) const;

	// Called by UCharacterAttributeSet before damage lands: shields reduce / reflect it, brands amplify it and detonate
	float ModifyIncomingDamage(UAbilitySystemComponent& TargetASC, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags, float Damage);

	/**
	 * A copy of Source playing on SlotName instead of its own slot, made once per world (the asset is untouched).
	 * A muted copy keeps its notifies but never fires them, for a second copy that plays in sync with the first:
	 * hits, sounds and combo windows must only happen once.
	 */
	UAnimMontage* GetMontageVariant(UAnimMontage* Source, FName SlotName, bool bMuteNotifies);

	// The asset a variant was copied from; Montage itself when it isn't a variant
	static UAnimMontage* GetMontageSource(UAnimMontage* Montage);

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

	struct FAegisState
	{
		TWeakObjectPtr<AActor> Target;
		FBeyondAegisSettings Settings;
		TWeakObjectPtr<UFXSystemComponent> Aura;
		FTimerHandle Timer;
	};
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FAegisState> Aegises;

	// "<source path>|<slot>|<muted>" -> variant
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimMontage>> MontageVariants;

	// Variant -> the asset it was copied from
	UPROPERTY(Transient)
	TMap<TObjectPtr<UAnimMontage>, TObjectPtr<UAnimMontage>> VariantSources;
};
