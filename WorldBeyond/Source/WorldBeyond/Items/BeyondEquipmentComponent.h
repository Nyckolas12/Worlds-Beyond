// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Items/BeyondItemTypes.h"
#include "BeyondEquipmentComponent.generated.h"

class ABeyondCharacterBase;
class UAbilitySystemComponent;
class UBeyondEquipmentComponent;
class UBeyondInventoryComponent;
class UFXSystemComponent;
class UGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondEquipmentChangedSignature, UBeyondEquipmentComponent*, Equipment);

/**
 * What a demigod wears: four armor slots and a weapon (created at runtime on Player-team characters). Item stats and
 * 2-piece set bonuses go on as one infinite UBeyondGE_EquipmentStats; full sets (4 pieces) switch on their effect:
 * - Venom Infusion: using Q / E / R coats weapon and spells in venom for a while, and infused hits poison;
 * - Chain Lightning: hits may arc lightning to nearby enemies;
 * - Sunfire: hits burn, and above a health threshold the wearer takes less damage.
 * Set effects run on UBeyondCombatSubsystem's hit feed; their own damage (DamageType.Proc.*) never triggers them.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondEquipmentComponent();

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FBeyondEquipmentChangedSignature OnEquipmentChanged;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool GetEquipped(EBeyondItemSlot Slot, FBeyondItemInstance& OutItem) const;

	// Whether this demigod can use it (weapon type); OutReason says why not
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanEquip(const FBeyondItemInstance& Item, FText& OutReason) const;

	// Takes an item out of the party bag and wears it; what was in its slot goes back to the bag
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool EquipFromInventory(FGuid ItemId);

	// Wears an item that isn't in the bag (starter kit); what was in its slot goes to the bag
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool EquipItem(const FBeyondItemInstance& Item);

	// Takes a slot's item off into the bag
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool Unequip(EBeyondItemSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetSetPieceCount(const UBeyondArmorSet* Set) const;

	// The worn set with this full-set effect and all its pieces on, if any
	const UBeyondArmorSet* FindFullSet(EBeyondSetEffect Effect) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasFullSet(EBeyondSetEffect Effect) const { return FindFullSet(Effect) != nullptr; }

	// Every set with at least one piece on
	TArray<const UBeyondArmorSet*> GetWornSets() const;

	// Items plus active 2-piece bonuses
	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetStatBonus(EBeyondSkillStat Stat) const;

	const TMap<EBeyondItemSlot, FBeyondItemInstance>& GetEquippedItems() const { return Equipped; }

	// Puts saved equipment back (no bag involved)
	void RestoreEquipped(const TMap<EBeyondItemSlot, FBeyondItemInstance>& SavedItems);

	// Sunfire: damage taken multiplier (1 when it doesn't apply)
	float GetDamageTakenMultiplier() const;

	// Venom Infusion is running
	UFUNCTION(BlueprintPure, Category = "Equipment|Sets")
	bool IsInfused() const;

	// Starts Venom Infusion now (normally a Q / E / R use does); false without the full set or while it cools down
	UFUNCTION(BlueprintCallable, Category = "Equipment|Sets")
	bool StartInfusion();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ABeyondCharacterBase* GetCharacter() const;
	UBeyondInventoryComponent* GetInventory() const;
	void ApplyEffects();
	void EndInfusion();
	bool IsFromWearer(const AActor* DamageInstigator) const;
	// Set-effect damage grows with the wearer's level (+10 % per level above 1)
	float GetProcScale() const;

	void HandleHitLanded(AActor* DamageInstigator, AActor* Target, float Damage, const FGameplayTagContainer& DamageTags);
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void ApplyDamageOverTime(AActor* Target, const FBeyondSetEffectSettings& Settings, const FGameplayTag& DamageTag,
		TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle>& Active);
	void ChainLightning(AActor* Target, const FBeyondSetEffectSettings& Settings);

	UPROPERTY(VisibleInstanceOnly, Category = "Equipment")
	TMap<EBeyondItemSlot, FBeyondItemInstance> Equipped;

	FActiveGameplayEffectHandle StatsHandle;
	FDelegateHandle HitLandedHandle;
	FDelegateHandle AbilityActivatedHandle;

	// World times are doubles: a float copy of "now" can round up and still count as in the future
	double InfusedUntil = -1.0;
	double NextInfusionTime = 0.0;
	double NextChainTime = 0.0;
	TWeakObjectPtr<UFXSystemComponent> InfusionAura;
	FTimerHandle InfusionTimer;

	// One poison and one burn per enemy: a new hit replaces (refreshes) the old one
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> PoisonEffects;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> BurnEffects;
};
