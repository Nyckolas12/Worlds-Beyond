// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BeyondCharacterBase.h"
#include "BeyondEnemyCharacter.generated.h"

class UBeyondAffixComponent;
class UBeyondAffixDefinition;
class UBeyondEnemyDefinition;
class UMaterialInstanceDynamic;

/**
 * Every Plan 3 enemy: an ABeyondCharacterBase driven by a UBeyondEnemyDefinition (looks, stats, abilities, reactions,
 * brain) and ABeyondEnemyController. Spawn it through UBeyondEnemySubsystem::SpawnEnemy (or place it with a Definition)
 * so the definition is applied before its abilities and stats are set up.
 */
UCLASS(Blueprintable)
class WORLDBEYOND_API ABeyondEnemyCharacter : public ABeyondCharacterBase
{
	GENERATED_BODY()

public:
	ABeyondEnemyCharacter();

	// What this enemy is (set before it spawns, or on a placed instance)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UBeyondEnemyDefinition> Definition;

	// An elite: tougher (Project Settings -> Worlds Beyond Enemies) and rolls affixes when none are set
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ExposeOnSpawn = "true"))
	bool bElite = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ExposeOnSpawn = "true"))
	TArray<TObjectPtr<UBeyondAffixDefinition>> Affixes;

	// Called up by another enemy (summons, Brood copies, boss clones): drops nothing and is worth no EXP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bSummoned = false;

	// Max health multiplier on top of the definition (Brood copies, boss clones)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.01"))
	float HealthScale = 1.0f;

	// Uniform scale multiplier on top of the definition
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.1"))
	float SizeScale = 1.0f;

	// Damage it deals x this (boss clones)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0"))
	float OutgoingDamageScale = 1.0f;

	// A shadow copy of a boss: no boss bar, no phases, no loot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	bool bClone = false;

	// Fighting walk speed x this (an enraged boss)
	UPROPERTY(BlueprintReadOnly, Category = "Enemy")
	float CombatSpeedScale = 1.0f;

	// Overlay tint (alpha = strength); replaces the region / affix tint
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void SetTintColor(const FLinearColor& Color);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UBeyondEnemyDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UBeyondAffixComponent* GetAffixComponent() const { return AffixComponent; }

	// "Molten Raider", the plates' and the console's name
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FText GetEnemyName() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsElite() const { return bElite || !Affixes.IsEmpty(); }

	// Where it spawned: it wanders around here and walks back here when it gives up a fight
	UFUNCTION(BlueprintPure, Category = "Enemy")
	FTransform GetHomeTransform() const { return HomeTransform; }

	void SetHomeTransform(const FTransform& NewHome) { HomeTransform = NewHome; bHomeSet = true; }

	// The definition's hit reaction for a response (Light when the response has none); null when it doesn't react
	UAnimMontage* GetHitReactMontage(const FGameplayTag& Response) const;

	// Seconds since it last took damage (plates, combat checks)
	float GetTimeSinceDamaged() const;

	// Back to full health without its fight state (walking home, party wipe)
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void RestoreToFull();

	//~ ABeyondCharacterBase
	virtual bool CanDropLoot() const override { return !bSummoned && !bClone; }
	virtual float GetOutgoingDamageScale() const override { return OutgoingDamageScale; }
	virtual float ModifyDamageTaken(float Damage, AActor* DamageInstigator, const FGameplayTagContainer& DamageTags) const override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath_Implementation() override;

	// Sets everything the definition, elite status and affixes decide, before the ability system reads it
	virtual void ApplyDefinition();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UBeyondAffixComponent> AffixComponent;

private:
	void ApplyTint();

	UFUNCTION()
	void HandleSelfHitTaken(ABeyondCharacterBase* HitCharacter, AActor* DamageInstigator, float Damage, FGameplayTag HitResponse);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TintMaterial;

	FTransform HomeTransform;
	bool bHomeSet = false;
	bool bDefinitionApplied = false;
	float LastDamagedTime = -1000.0f;
};
