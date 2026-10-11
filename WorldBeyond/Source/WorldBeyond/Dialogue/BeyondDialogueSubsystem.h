// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeyondDialogueSubsystem.generated.h"

class ABeyondCharacterBase;
class ACharacter;
class UActorComponent;
class UBeyondPartyComponent;

UENUM(BlueprintType)
enum class EBeyondConversationKind : uint8
{
	// Face to face with an NPC (F)
	Talk,
	// Angel and Ji-Woong's lines at the bottom of the screen while you play
	Banter
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBeyondConversationSignature, AActor*, Speaker, AActor*, Partner, FName, StartRow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondDialogueRowSignature, FName, Row);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondDialogueEventSignature, FName, EventName, FName, Row);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondBossDefeatedSignature, FName, BossId);

/**
 * Plan 4's hub on top of the Advanced Dialogue System pack (see BeyondDialogueBridge):
 * - one conversation at a time, run on the leader's BP_AC_Dialogue; a face-to-face talk closes running banter
 * - every row's Special Event (DT_Dialogue column, ';' between several): Flag.X / Unflag.X set and clear the story
 *   flag X; anything else is broadcast as OnDialogueEvent for Blueprints and level scripts
 * - story flags live on the party (saved); Boss.<BossId> is raised whenever a boss falls (mini-bosses too)
 */
UCLASS()
class WORLDBEYOND_API UBeyondDialogueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBeyondDialogueSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// Runs Row (DT_Dialogue) on Speaker's BP_AC_Dialogue; Partner is the NPC (null for banter)
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartConversation(ACharacter* Speaker, FName Row, AActor* Partner, EBeyondConversationKind Kind);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void EndConversation();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsConversationActive() const { return bActive; }

	// Face to face with an NPC
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsTalking() const { return bActive && Kind == EBeyondConversationKind::Talk; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsBanterActive() const { return bActive && Kind == EBeyondConversationKind::Banter; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FName GetCurrentRow() const;

	// The row the conversation started from
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FName GetStartRow() const { return bActive ? StartRow : NAME_None; }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	AActor* GetSpeaker() const { return Speaker.Get(); }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	AActor* GetPartner() const { return Partner.Get(); }

	// The BP_AC_Dialogue running the conversation
	UActorComponent* GetActiveComponent() const { return ActiveComponent.Get(); }

	//~ Story flags (saved with the party)

	UFUNCTION(BlueprintPure, Category = "Dialogue|Flags")
	bool HasFlag(FName Flag) const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue|Flags")
	void SetFlag(FName Flag, bool bSet = true);

	// Every Required flag is set and no Blocked one is
	UFUNCTION(BlueprintPure, Category = "Dialogue|Flags")
	bool PassesFlags(const TArray<FName>& Required, const TArray<FName>& Blocked) const;

	// What a row's Special Event does (also callable by hand)
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void RunSpecialEvent(FName Event, FName Row = NAME_None);

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FBeyondConversationSignature OnConversationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FBeyondConversationSignature OnConversationEnded;

	// A row came on screen
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FBeyondDialogueRowSignature OnDialogueRow;

	// A Special Event that isn't a flag (e.g. "OpenGate")
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FBeyondDialogueEventSignature OnDialogueEvent;

	// A boss fell (clones and summons don't count)
	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FBeyondBossDefeatedSignature OnBossDefeated;

	// Bound to WBP_Dialogue's UpdatedCurrentRow
	UFUNCTION()
	void HandleRowChanged(FName RowName);

private:
	UFUNCTION()
	void HandleCharacterKilled(ABeyondCharacterBase* Victim, AActor* Killer);

	UBeyondPartyComponent* GetParty() const;
	void HandleRow(FName Row);
	void Poll();
	void Finish();

	TWeakObjectPtr<UActorComponent> ActiveComponent;
	TWeakObjectPtr<AActor> Speaker;
	TWeakObjectPtr<AActor> Partner;
	EBeyondConversationKind Kind = EBeyondConversationKind::Talk;
	FName StartRow;
	FName LastRow;
	bool bActive = false;
	// Rows come from the widget's delegate; without it they are polled
	bool bRowsBound = false;
	FTimerHandle PollTimer;
};
