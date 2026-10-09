// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BeyondPartyComponent.generated.h"

class ABeyondCharacterBase;
class ABeyondCompanionController;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondLeaderChangedSignature, ABeyondCharacterBase*, NewLeader, ABeyondCharacterBase*, OldLeader);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondReviveProgressSignature, ABeyondCharacterBase*, DownedMember, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBeyondPartyWipedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondBondChangedSignature, float, Bond, float, MaxBond);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondMemberLevelUpSignature, ABeyondCharacterBase*, Member, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondExperienceAwardedSignature, ABeyondCharacterBase*, Victim, float, Experience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondBondPointsChangedSignature, int32, BondPoints);

class UBeyondEquipmentComponent;
class UBeyondInventoryComponent;
class UBeyondSkillTreeComponent;

/**
 * The two demigods: the player controls the leader, the other is driven by a companion controller.
 * Handles swapping, auto-swap when the leader falls, proximity revives, party wipes, the shared
 * Bond meter that charges the duo super move, shared EXP and loot from kills, the starter kit and saving the party's
 * progress (levels, skill trees, the bag and what everyone wears).
 * Lives on ABeyondPlayerController.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondPartyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondPartyComponent();

	/**
	 * Party members. Instances already placed in the level are reused, missing ones are spawned
	 * next to the leader. Leave empty to use every Player-team Beyond character in the level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party")
	TArray<TSubclassOf<ABeyondCharacterBase>> PartyClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party")
	TSubclassOf<ABeyondCompanionController> CompanionControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Swap", meta = (ClampMin = "0"))
	float SwapCooldown = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Swap", meta = (ClampMin = "0"))
	float CameraBlendTime = 0.35f;

	// Swap to the other demigod this long after the leader dies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Swap", meta = (ClampMin = "0"))
	float AutoSwapDelayOnDeath = 1.5f;

	// Stand this close to a downed demigod to revive it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Revive", meta = (ClampMin = "0"))
	float ReviveRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Revive", meta = (ClampMin = "0.1"))
	float ReviveTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Revive", meta = (ClampMin = "0.05", ClampMax = "1"))
	float ReviveHealthFraction = 0.3f;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FBeyondLeaderChangedSignature OnLeaderChanged;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FBeyondReviveProgressSignature OnReviveProgress;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FBeyondPartyWipedSignature OnPartyWiped;

	// Bond: fills as the demigods fight, spent by the duo super move
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond", meta = (ClampMin = "1"))
	float MaxBond = 100.0f;

	// Per point of damage a party member deals
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond", meta = (ClampMin = "0"))
	float BondPerDamageDealt = 0.2f;

	// Per point of damage a party member takes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond", meta = (ClampMin = "0"))
	float BondPerDamageTaken = 0.1f;

	// Bonus when both demigods hit the same enemy within SynergyWindow seconds
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond", meta = (ClampMin = "0"))
	float SynergyBond = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond", meta = (ClampMin = "0"))
	float SynergyWindow = 3.0f;

	UPROPERTY(BlueprintAssignable, Category = "Party|Bond")
	FBeyondBondChangedSignature OnBondChanged;

	// Any member levelled up from EXP (the HUD shows the banner)
	UPROPERTY(BlueprintAssignable, Category = "Party|Progression")
	FBeyondMemberLevelUpSignature OnMemberLevelUp;

	// The party was given EXP for a kill (each member got Experience)
	UPROPERTY(BlueprintAssignable, Category = "Party|Progression")
	FBeyondExperienceAwardedSignature OnExperienceAwarded;

	// Save levels / EXP to the "BeyondProgress" slot and load them on start (also needs the Beyond.SaveProgress console variable)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Progression")
	bool bSaveProgress = true;

	// Gives every member the same EXP (a downed member gets it too)
	UFUNCTION(BlueprintCallable, Category = "Party|Progression")
	void AwardExperience(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Party|Progression")
	bool SaveProgress();

	UFUNCTION(BlueprintCallable, Category = "Party|Progression")
	bool LoadProgress();

	// Deletes the save and puts every member back to level 1 (bag emptied, starter kit again)
	UFUNCTION(BlueprintCallable, Category = "Party|Progression")
	void ResetProgress();

	// Story bosses the party has beaten (saved): their arenas don't bring them back
	UFUNCTION(BlueprintPure, Category = "Party|Bosses")
	bool IsBossDefeated(FName BossId) const { return DefeatedBosses.Contains(BossId); }

	UFUNCTION(BlueprintCallable, Category = "Party|Bosses")
	void MarkBossDefeated(FName BossId);

	// Every story boss back (console Beyond.ResetBosses)
	UFUNCTION(BlueprintCallable, Category = "Party|Bosses")
	void ResetDefeatedBosses();

	// Defeated enemies drop loot (UBeyondLootSettings rules plus their Guaranteed Loot)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Items")
	bool bDropLoot = true;

	// The party's bag (the inventory component on the same player controller)
	UFUNCTION(BlueprintPure, Category = "Party|Items")
	UBeyondInventoryComponent* GetInventory() const;

	// Equips the starter items (Project Settings -> Worlds Beyond Loot) on every demigod with that slot free; given
	// once, the first time the party forms without a save
	UFUNCTION(BlueprintCallable, Category = "Party|Items")
	void GiveStarterKit();

	// Rolls Victim's loot and drops it at its feet; returns how many items dropped
	UFUNCTION(BlueprintCallable, Category = "Party|Items")
	int32 DropLoot(ABeyondCharacterBase* Victim);

	// bSaveProgress and the Beyond.SaveProgress console variable
	UFUNCTION(BlueprintPure, Category = "Party|Progression")
	bool IsSavingEnabled() const;

	// The highest level in the party (the duo tree's level gates use it)
	UFUNCTION(BlueprintPure, Category = "Party|Progression")
	int32 GetPartyLevel() const;

	// Bond Points: the duo tree's currency, shared by the party
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond Points", meta = (ClampMin = "1"))
	int32 LevelsPerBondPoint = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Party|Bond Points", meta = (ClampMin = "0"))
	int32 BondPointsPerBoss = 1;

	UPROPERTY(BlueprintAssignable, Category = "Party|Bond Points")
	FBeyondBondPointsChangedSignature OnBondPointsChanged;

	UFUNCTION(BlueprintPure, Category = "Party|Bond Points")
	int32 GetBondPoints() const { return BondPoints; }

	UFUNCTION(BlueprintCallable, Category = "Party|Bond Points")
	void AddBondPoints(int32 Amount);

	// False (and nothing taken) if there aren't enough
	UFUNCTION(BlueprintCallable, Category = "Party|Bond Points")
	bool SpendBondPoints(int32 Amount);

	// Set by the duo skill tree: Bond gain multiplier and the fraction the meter keeps after a duo move
	void SetBondModifiers(float GainMultiplier, float EchoFraction);

	UFUNCTION(BlueprintPure, Category = "Party|Bond")
	float GetBondGainMultiplier() const { return BondGainMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Party|Bond")
	float GetBond() const { return Bond; }

	UFUNCTION(BlueprintPure, Category = "Party|Bond")
	bool IsBondFull() const { return Bond >= MaxBond; }

	UFUNCTION(BlueprintCallable, Category = "Party|Bond")
	void AddBond(float Amount);

	// Empties the meter if it is full; returns whether it was
	UFUNCTION(BlueprintCallable, Category = "Party|Bond")
	bool ConsumeBond();

	// The party member behind an actor (itself, or the owner / instigator of its projectile or weapon)
	UFUNCTION(BlueprintPure, Category = "Party")
	ABeyondCharacterBase* FindMemberFor(const AActor* Actor) const;

	// Called by the player controller once it possesses its first pawn
	void InitializeParty(APawn* InitialLeader);

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool SwapLeader();

	UFUNCTION(BlueprintPure, Category = "Party")
	ABeyondCharacterBase* GetLeader() const;

	// The member the player isn't controlling (first one, if more than two)
	UFUNCTION(BlueprintPure, Category = "Party")
	ABeyondCharacterBase* GetCompanion() const;

	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<ABeyondCharacterBase*> GetMembers() const;

	// Revive / heal everyone and move them to Transform (checkpoint respawn)
	UFUNCTION(BlueprintCallable, Category = "Party")
	void RespawnPartyAt(const FTransform& Transform);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleMemberKilled(ABeyondCharacterBase* Member, AActor* Killer);

	UFUNCTION()
	void HandleDamageDealt(AActor* DamageInstigator, AActor* Target, float Damage);

	UFUNCTION()
	void HandleCharacterKilled(ABeyondCharacterBase* Victim, AActor* Killer);

	UFUNCTION()
	void HandleMemberLevelUp(ABeyondCharacterBase* Member, int32 NewLevel);

	UFUNCTION()
	void HandleSkillTreeChanged(UBeyondSkillTreeComponent* Tree);

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleEquipmentChanged(UBeyondEquipmentComponent* Equipment);

private:
	APlayerController* GetPlayerController() const;
	void AddMember(ABeyondCharacterBase* Member);
	// Skill tree and equipment exist once a member is possessed; listen to them for saving
	void BindMemberComponents(ABeyondCharacterBase* Member);
	// Several changes in one frame (equipping takes from the bag and puts on) write the save once, next frame
	void QueueSave();
	void FlushQueuedSave();
	void GiveToCompanionController(ABeyondCharacterBase* Member, ABeyondCharacterBase* NewLeader);
	ABeyondCharacterBase* FindNextAliveMember(const ABeyondCharacterBase* After) const;
	bool SwapTo(ABeyondCharacterBase* NewLeader, bool bIgnoreCooldown);
	void AutoSwapAfterDeath();
	void UpdateRevive(float DeltaTime);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABeyondCharacterBase>> Members;

	void SetBond(float NewBond);

	struct FRecentHit
	{
		TWeakObjectPtr<ABeyondCharacterBase> Member;
		float Time = 0.0f;
		float LastSynergyTime = -1000.0f;
	};
	TMap<TWeakObjectPtr<AActor>, FRecentHit> RecentHits;
	float Bond = 0.0f;

	TWeakObjectPtr<ABeyondCharacterBase> ReviveTarget;
	float ReviveProgress = 0.0f;
	float LastSwapTime = -1000.0f;
	bool bInitialized = false;
	// Nothing is written before the save was read, so a fresh party can't overwrite it
	bool bProgressLoaded = false;
	// Restoring a save changes trees and levels one at a time; nothing is written until it is done
	bool bRestoringProgress = false;

	UBeyondSkillTreeComponent* GetDuoTree() const;
	void AwardBondPointsForLevel(int32 Level);

	int32 BondPoints = 0;
	int32 BondPointsLevel = 0;
	bool bStarterKitGiven = false;
	TSet<FName> DefeatedBosses;
	bool bSaveQueued = false;
	float BondGainMultiplier = 1.0f;
	float BondEchoFraction = 0.0f;
	FTimerHandle AutoSwapTimer;
};
