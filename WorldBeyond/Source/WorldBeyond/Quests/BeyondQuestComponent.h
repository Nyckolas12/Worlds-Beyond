// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/BeyondQuestTypes.h"
#include "BeyondQuestComponent.generated.h"

class ABeyondCharacterBase;
class ABeyondWaystone;
class UBeyondPartyComponent;
class UBeyondSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondQuestSignature, FName, QuestId);

/**
 * The party's quests (Plan 6; on the player controller, saved with the party). Quests start on their own once their
 * prerequisites are met and complete when every objective is; the rewards come once, on completion. It re-checks on the
 * game's events: story flags (dialogue, bosses, discovery), level-ups, Bond levels, kills, the bag, conversations.
 * Beyond.Quests turns it off (0) or on (1); by default (-1) it is on except while automation tests run, so their
 * flags, kills and EXP checks don't set quests off.
 */
UCLASS(ClassGroup = (Beyond), meta = (BlueprintSpawnableComponent))
class WORLDBEYOND_API UBeyondQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBeyondQuestComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests")
	TSoftObjectPtr<UBeyondQuestDatabase> Database;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FBeyondQuestSignature OnQuestStarted;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FBeyondQuestSignature OnQuestCompleted;

	// An objective moved (a kill counted, a flag set)
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FBeyondQuestSignature OnQuestUpdated;

	// Beyond.Quests (see the class comment)
	UFUNCTION(BlueprintPure, Category = "Quests")
	static bool IsQuestTrackingEnabled();

	UFUNCTION(BlueprintPure, Category = "Quests")
	UBeyondQuestDatabase* GetDatabase() const;

	// Uses another database from now on (tests); progress is kept per quest id
	void SetDatabase(UBeyondQuestDatabase* NewDatabase);

	const FBeyondQuest* FindQuest(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	EBeyondQuestState GetQuestState(FName QuestId) const;

	// How far an objective is: kills, items, levels; 0 or 1 for flags and talks
	UFUNCTION(BlueprintPure, Category = "Quests")
	int32 GetObjectiveProgress(FName QuestId, int32 ObjectiveIndex) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	bool IsObjectiveDone(FName QuestId, int32 ObjectiveIndex) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	int32 GetQuestCount(EBeyondQuestKind Kind) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	int32 GetCompletedCount(EBeyondQuestKind Kind) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	FName GetTrackedQuest() const { return TrackedQuest; }

	// The quest the HUD follows (none: stop tracking); only active quests can be tracked
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool SetTrackedQuest(FName QuestId);

	// What the HUD and the Overview show: the tracked quest, else the earliest active main quest, else any active one
	UFUNCTION(BlueprintPure, Category = "Quests")
	FName GetFocusQuest() const;

	// The first objective of an active quest that isn't done (INDEX_NONE if none)
	int32 GetCurrentObjective(FName QuestId) const;

	// Checks every quest now (the game's events call it)
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void Refresh();

	// Completes a quest now, whatever its objectives (cheats); bGrantRewards: give its rewards too
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool CompleteQuest(FName QuestId, bool bGrantRewards = true);

	// Forgets every quest's progress (new game, Beyond.Quest reset)
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void ResetQuests();

	// Saving and loading (the party calls them); saves from before quests (version 6) catch up without rewards
	void WriteSave(UBeyondSaveGame& Save) const;
	void ReadSave(const UBeyondSaveGame& Save);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleStoryFlagChanged(FName Flag, bool bSet);
	UFUNCTION()
	void HandleMemberLevelUp(ABeyondCharacterBase* Member, int32 NewLevel);
	UFUNCTION()
	void HandleBondLevelChanged(int32 BondLevel);
	UFUNCTION()
	void HandleCharacterKilled(ABeyondCharacterBase* Victim, AActor* Killer);
	UFUNCTION()
	void HandleInventoryChanged();
	UFUNCTION()
	void HandleDiscovered(FName Id, FText DisplayName);
	UFUNCTION()
	void HandleWaystoneAttuned(ABeyondWaystone* Waystone);
	UFUNCTION()
	void HandleConversationStarted(AActor* Speaker, AActor* Partner, FName StartRow);
	UFUNCTION()
	void HandlePartyFormed();

	UBeyondPartyComponent* GetParty() const;
	bool AreRequirementsMet(const FBeyondQuest& Quest) const;
	int32 MeasureObjective(const FBeyondQuest& Quest, int32 ObjectiveIndex) const;
	FBeyondQuestProgress& FindOrAddProgress(const FBeyondQuest& Quest);
	void FinishQuest(const FBeyondQuest& Quest, bool bGrantRewards);
	void GrantRewards(const FBeyondQuest& Quest);
	// Adds to a counted objective (kills, talks) of every active quest that waits for it
	void CountObjective(EBeyondObjectiveType Type, FName Target);

	UPROPERTY(VisibleInstanceOnly, Category = "Quests")
	TMap<FName, FBeyondQuestProgress> Progress;

	UPROPERTY(Transient)
	TObjectPtr<UBeyondQuestDatabase> DatabaseOverride;

	// Active quests already announced (OnQuestStarted fires once per quest and session)
	TSet<FName> Announced;
	FName TrackedQuest;
	bool bRefreshing = false;
	// A save from before quests: the next check completes what is already done, without rewards or announcements
	bool bCatchingUp = false;
};
