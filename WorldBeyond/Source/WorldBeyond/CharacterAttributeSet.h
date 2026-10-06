// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.generated.h"
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName)\
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName,PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)\
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// Instigator, Causer, Magnitude, HitResponse (an Event.Hit.* tag, or empty when no reaction should play)
DECLARE_MULTICAST_DELEGATE_FourParams(FBeyondAttributeEvent, AActor* /*Instigator*/, AActor* /*Causer*/, float /*Magnitude*/, FGameplayTag /*HitResponse*/);

/**
 * Health, stamina and combat stats for every combatant (demigods and enemies).
 * Damage and healing should go through the IncomingDamage / IncomingHeal meta attributes
 * (see UBeyondCombatLibrary) so blocking, parrying, invincibility and death are handled in one place.
 */
UCLASS()
class WORLDBEYOND_API UCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UCharacterAttributeSet();
	//Health Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_CurrentHealth)
	FGameplayAttributeData CurrentHealth;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CurrentHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxHealth)

	//Stamina Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_CurrentStamina)
	FGameplayAttributeData CurrentStamina;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CurrentStamina)

	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxStamina)

	// Combat stats: Strength scales melee damage, Arcana ability / magic damage (UBeyondCombatLibrary::ApplyDamage),
	// Defense reduces damage taken (x 100 / (100 + Defense))
	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Strength)
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Strength)

	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Arcana)
	FGameplayAttributeData Arcana;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Arcana)

	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Defense)

	// Character level (demigods level up through UBeyondProgressionAttributeSet; enemies keep their Starting Level)
	UPROPERTY(BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_Level)
	FGameplayAttributeData Level;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Level)

	//Meta Attributes - temporary values turned into health changes in PostGameplayEffectExecute
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, IncomingHeal)

	// Broadcast after damage (including blocked / parried hits with Magnitude 0)
	mutable FBeyondAttributeEvent OnHitTaken;

	// Broadcast once when CurrentHealth reaches zero
	mutable FBeyondAttributeEvent OnOutOfHealth;

protected:
	UFUNCTION()
	virtual void OnRep_CurrentHealth(const FGameplayAttributeData& OldCurrentHealth) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, CurrentHealth, OldCurrentHealth);
	}
	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, MaxHealth, OldMaxHealth);
	}

	UFUNCTION()
	virtual void OnRep_CurrentStamina(const FGameplayAttributeData& OldCurrentStamina) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, CurrentStamina, OldCurrentStamina);
	}
	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, MaxStamina, OldMaxStamina);
	}
	UFUNCTION()
	virtual void OnRep_Strength(const FGameplayAttributeData& OldStrength) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Strength, OldStrength);
	}
	UFUNCTION()
	virtual void OnRep_Arcana(const FGameplayAttributeData& OldArcana) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Arcana, OldArcana);
	}
	UFUNCTION()
	virtual void OnRep_Defense(const FGameplayAttributeData& OldDefense) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Defense, OldDefense);
	}
	UFUNCTION()
	virtual void OnRep_Level(const FGameplayAttributeData& OldLevel) const
	{
		GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSet, Level, OldLevel);
	}

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	void HandleIncomingDamage(const FGameplayEffectModCallbackData& Data);
	void HandleIncomingHeal(const FGameplayEffectModCallbackData& Data);
	void CheckOutOfHealth(const FGameplayEffectModCallbackData& Data);

	// Set once OnOutOfHealth has fired, so death is only broadcast once
	bool bOutOfHealth = false;
};
