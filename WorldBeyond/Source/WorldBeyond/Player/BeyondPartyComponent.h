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

/**
 * The two demigods: the player controls the leader, the other is driven by a companion controller.
 * Handles swapping, auto-swap when the leader falls, proximity revives, party wipes, the shared
 * Bond meter that charges the duo super move, shared EXP from kills and saving the party's progress.
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

	// Deletes the save and puts every member back to level 1
	UFUNCTION(BlueprintCallable, Category = "Party|Progression")
	void ResetProgress();

	// bSaveProgress and the Beyond.SaveProgress console variable
	UFUNCTION(BlueprintPure, Category = "Party|Progression")
	bool IsSavingEnabled() const;

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

private:
	APlayerController* GetPlayerController() const;
	void AddMember(ABeyondCharacterBase* Member);
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
	FTimerHandle AutoSwapTimer;
};
