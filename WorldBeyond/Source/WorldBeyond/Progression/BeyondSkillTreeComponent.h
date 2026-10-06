// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Progression/BeyondSkillTree.h"
#include "BeyondSkillTreeComponent.generated.h"

class ABeyondCharacterBase;
class UBeyondPartyComponent;
class UBeyondSkillTreeComponent;
class UGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBeyondSkillUnlockedSignature, UBeyondSkillTreeComponent*, Tree, FName, NodeId, int32, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondSkillTreeChangedSignature, UBeyondSkillTreeComponent*, Tree);

/**
 * A demigod's skill tree: ranks bought with the demigod's skill points (one per level, Plan 1A) and applied through
 * GAS. Stat nodes add one infinite UBeyondGE_SkillStats; ability-rank nodes raise the level of the matching ability
 * specs (more damage, shorter cooldowns); grant nodes give abilities. Created at runtime on Player-team characters
 * from their Skill Tree asset; the party saves the ranks.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondSkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondSkillTreeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree")
	TObjectPtr<UBeyondSkillTreeAsset> Tree;

	UPROPERTY(BlueprintAssignable, Category = "Skill Tree")
	FBeyondSkillUnlockedSignature OnSkillUnlocked;

	// Any change: unlock, reset, restore, duo loadout
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree")
	FBeyondSkillTreeChangedSignature OnSkillTreeChanged;

	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	void SetTree(UBeyondSkillTreeAsset* NewTree);

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetRank(FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	EBeyondSkillNodeState GetNodeState(FName NodeId) const;

	// Whether the next rank can be bought now; OutReason says why not ("Requires level 8", "Needs 2 Skill Points")
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool CanUnlock(FName NodeId, FText& OutReason) const;

	// Buys the next rank and applies it
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool Unlock(FName NodeId);

	// Refunds every point spent in this tree; returns how many
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	int32 ResetTree();

	// Points that can be spent here (skill points, or the party's Bond Points for the duo tree)
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	virtual int32 GetAvailablePoints() const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetSpentPoints() const;

	// The level gates compare against this: the demigod's level (party level for the duo tree)
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	virtual int32 GetTreeLevel() const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	FText GetCurrencyName() const;

	// Sum of a stat over the ranks taken
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	float GetStatBonus(EBeyondSkillStat Stat) const;

	// Sum of Value x rank over every effect of this type (Bond Gain, Bond Echo)
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	float GetEffectTotal(EBeyondSkillEffectType Type) const;

	// Levels added to AbilityClass by ability-rank nodes
	int32 GetAbilityRankBonus(const UClass* AbilityClass) const;

	// Abilities from effects of this type on nodes with a rank (granted abilities, unlocked duo powers)
	TArray<TSubclassOf<UGameplayAbility>> GetUnlockedAbilities(EBeyondSkillEffectType Type) const;

	const TMap<FName, int32>& GetRanks() const { return Ranks; }

	// Puts saved ranks back without spending points (the saved points are restored separately)
	void RestoreRanks(const TMap<FName, int32>& SavedRanks);

	// Re-applies every effect (cheap; after abilities were granted or the party changed)
	void RefreshEffects() { ApplyEffects(); }

protected:
	virtual bool PayPoints(int32 Amount);
	virtual void RefundPoints(int32 Amount);
	virtual void ApplyEffects();

	// Sets the level of Character's ability specs this tree upgrades (abilities it doesn't upgrade are left alone)
	void ApplyAbilityRanks(ABeyondCharacterBase* Character) const;

	bool MeetsRequirements(const FBeyondSkillNode& Node) const;
	void BroadcastChanged();
	ABeyondCharacterBase* GetCharacter() const;

	UPROPERTY(VisibleInstanceOnly, Category = "Skill Tree")
	TMap<FName, int32> Ranks;

private:
	bool TargetsAbility(const UClass* AbilityClass) const;
	void ApplyStats(ABeyondCharacterBase* Character);
	bool ApplyGrantedAbilities(ABeyondCharacterBase* Character);

	FActiveGameplayEffectHandle StatsHandle;
	TMap<UClass*, FGameplayAbilitySpecHandle> GrantedAbilities;
};

/**
 * The duo tree, shared by the party and paid with Bond Points (UBeyondPartyComponent). Lives on the player controller.
 * Its nodes unlock duo powers, rank up the duo moves on both demigods and modify the Bond meter. The duo loadout
 * picks which unlocked power the duo slot (G) fires; the others stay granted on Ability.Input.Unbound.
 */
UCLASS(ClassGroup = (Beyond))
class WORLDBEYOND_API UBeyondDuoSkillTreeComponent : public UBeyondSkillTreeComponent
{
	GENERATED_BODY()

public:
	virtual int32 GetAvailablePoints() const override;
	virtual int32 GetTreeLevel() const override;

	// The duo power on G (the tree's starter until another is chosen)
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Duo")
	TSubclassOf<UGameplayAbility> GetDuoLoadout() const;

	// Puts an unlocked duo power on G; false if it isn't unlocked
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Duo")
	bool SetDuoLoadout(TSubclassOf<UGameplayAbility> DuoPower);

	// The starter plus every unlocked duo power
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Duo")
	TArray<TSubclassOf<UGameplayAbility>> GetAvailableDuoPowers() const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree|Duo")
	TSubclassOf<UGameplayAbility> GetStarterDuoPower() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool PayPoints(int32 Amount) override;
	virtual void RefundPoints(int32 Amount) override;
	virtual void ApplyEffects() override;

	UFUNCTION()
	void HandleLeaderChanged(ABeyondCharacterBase* NewLeader, ABeyondCharacterBase* OldLeader);

private:
	UBeyondPartyComponent* GetParty() const;
	void ApplyDuoPowers(ABeyondCharacterBase* Member);
	void ApplyLoadout(ABeyondCharacterBase* Member) const;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayAbility> Loadout;

	// Duo powers this component granted, per member
	TMap<TWeakObjectPtr<ABeyondCharacterBase>, TMap<UClass*, FGameplayAbilitySpecHandle>> GrantedPowers;
};
