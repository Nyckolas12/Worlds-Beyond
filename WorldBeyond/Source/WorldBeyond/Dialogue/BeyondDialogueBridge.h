// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AActor;
class ACharacter;
class UActorComponent;
class UDataTable;
class UUserWidget;

/** A DT_Dialogue row as C++ reads it (F_Dialogue) */
struct FBeyondDialogueRowView
{
	FName Row;
	FName NextRow;
	// DT_Speakers row of whoever says it
	FName SpeakerRow;
	// Our hook: Flag.X / Unflag.X / anything else (see UBeyondDialogueSubsystem)
	FName SpecialEvent;
	FText Text;
	// "free movement dialogue" (you keep playing) rather than "face to face"
	bool bFreeMovement = false;
	// The row shows choices instead of a line
	bool bOptions = false;
	TArray<TPair<FText, FName>> Options;
	// Free movement: seconds before the next line
	float SkipDuration = 0.0f;
};

/**
 * Drives the Advanced Dialogue System pack (Content/DialogueSystem) from C++ without editing its Blueprints, like
 * BeyondLegacyDamage does for BPC_DamageSystem: its functions are called and its variables read by name through
 * reflection. CheckPackBindings lists anything that went missing (the Dialogue test fails on it).
 *
 * Pieces of the pack:
 * - BP_AC_Dialogue (the player's): StartDialogue(RowName, Actor) runs a DT_Dialogue chain in WBP_MainDialogue
 * - BP_AC_DialogueOverHead (everyone's): StartDialogueReplicated(RowName, DialogueActors) runs DT_TextOverHead rows
 * - a WidgetComponent (WBP_TextOverHead) and a ChildActorComponent (BP_CameraActor), both tagged "dialogue"
 */
namespace BeyondDialogue
{
	// The actor's BP_AC_Dialogue / BP_AC_DialogueOverHead, if it has one
	UActorComponent* FindDialogueComponent(const AActor* Actor);
	UActorComponent* FindOverHeadComponent(const AActor* Actor);

	/**
	 * Adds what the pack needs to talk: the tagged over-head text widget and focus camera, the over-head component and,
	 * with bConversations (the party), BP_AC_Dialogue. Only what is missing; call before the actor's BeginPlay.
	 */
	void EnsureParticipant(ACharacter* Character, bool bConversations);

	//~ Conversations (BP_AC_Dialogue)

	// Runs Row's chain; Partner is the NPC talked to (null for the demigods' banter)
	bool StartConversation(UActorComponent* DialogueComponent, FName Row, AActor* Partner);

	// Ends the conversation the way the pack does (input back to the game, camera back on the speaker)
	bool CloseConversation(UActorComponent* DialogueComponent);

	// For either component
	bool IsInDialogue(const UActorComponent* Component);

	UUserWidget* GetMainWidget(const UActorComponent* DialogueComponent);
	UUserWidget* GetDialogueWidget(const UActorComponent* DialogueComponent);

	// The row on screen (None when none)
	FName GetCurrentRow(const UActorComponent* DialogueComponent);

	// The skip key: finishes the typing, or goes to the next row
	bool Skip(UActorComponent* DialogueComponent);

	// Choices on screen; Ready once they have finished typing
	int32 GetOptionCount(const UActorComponent* DialogueComponent, bool* bOutReady = nullptr);

	// Clicks a choice (what the mouse / gamepad does); false if it isn't there or still typing
	bool ChooseOption(UActorComponent* DialogueComponent, int32 Index);

	// Calls Listener's FunctionName(FName RowName) whenever a row starts (WBP_Dialogue's UpdatedCurrentRow)
	bool BindRowChanged(UActorComponent* DialogueComponent, UObject* Listener, FName FunctionName);

	//~ Over-head chatter (BP_AC_DialogueOverHead)

	// Others[i] says the rows with DialogueActorIndex i
	bool StartOverHead(UActorComponent* OverHeadComponent, FName Row, const TArray<AActor*>& Others);
	void StopOverHead(UActorComponent* OverHeadComponent);

	//~ NPC Blueprints implementing BP_I_Dialogue (BP_NPC_Base comes from the pack's BP_ExampleCharacter)

	void SetActorRowName(AActor* Actor, FName Row);
	bool IsActorInDialogue(const AActor* Actor);

	//~ Tables

	UDataTable* GetDialogueTable();
	UDataTable* GetOverHeadTable();
	UDataTable* GetSpeakersTable();

	bool ReadDialogueRow(FName Row, FBeyondDialogueRowView& Out);

	// A field of a row by its name in the table editor, as text (None / empty if missing)
	FString GetRowFieldText(const UDataTable* Table, FName Row, const TCHAR* FieldName);

	// Every pack function, parameter and variable C++ relies on that can't be found (empty: all there)
	TArray<FString> CheckPackBindings();
}
