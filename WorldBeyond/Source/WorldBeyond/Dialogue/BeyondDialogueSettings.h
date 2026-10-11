// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BeyondDialogueSettings.generated.h"

class UActorComponent;
class UBeyondBanterSet;
class UDataTable;
class UUserWidget;

/**
 * Dialogue (Project Settings -> Game -> Worlds Beyond Dialogue): where the Advanced Dialogue System pack lives, how
 * close you talk to NPCs, village chatter and the demigods' banter. Saved in Config/DefaultGame.ini.
 * The pack's Blueprints read their three data tables directly, so the tables here are only read by C++ (special
 * events, checks); our rows go into the pack's own tables.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond Dialogue"))
class WORLDBEYOND_API UBeyondDialogueSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBeyondDialogueSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	//~ The pack

	// BP_AC_Dialogue: runs DT_Dialogue conversations (the party gets one)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftClassPtr<UActorComponent> DialogueComponentClass;

	// BP_AC_DialogueOverHead: runs DT_TextOverHead chatter over heads (everyone who talks gets one)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftClassPtr<UActorComponent> OverHeadComponentClass;

	// WBP_TextOverHead, shown by the over-head widget component
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftClassPtr<UUserWidget> OverHeadWidgetClass;

	// BP_CameraActor, the focus camera a speaker carries (child actor component)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftClassPtr<AActor> FocusCameraClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftObjectPtr<UDataTable> DialogueTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftObjectPtr<UDataTable> OverHeadTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	TSoftObjectPtr<UDataTable> SpeakersTable;

	// The pack finds a speaker's widget and camera components by this tag
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pack")
	FName ComponentTag = TEXT("dialogue");

	//~ Speakers

	// Give the demigods the pack's components at runtime (their Blueprints need no edit)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Speakers")
	bool bGivePartyDialogueComponents = true;

	// Over-head text this far above the capsule's top
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Speakers")
	float OverHeadHeight = 35.0f;

	// The focus camera, from the capsule centre: in front of the face (X forward), looking back at it
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Speakers")
	FVector FocusCameraOffset = FVector(150.0f, 35.0f, 60.0f);

	// Banter's box moves by this much (screen units) so it sits above the ability bar and the Bond meter
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banter")
	FVector2D BanterBoxOffset = FVector2D(0.0f, -175.0f);

	//~ Talking

	// F talks to an NPC this close to the leader (capsule to capsule)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Talking", meta = (ClampMin = "50"))
	float TalkRange = 260.0f;

	// ...and no further than this from where the leader faces (degrees; the nearest NPC wins inside it)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Talking", meta = (ClampMin = "10", ClampMax = "180"))
	float TalkAngle = 80.0f;

	// No talking while an enemy this close to the leader is fighting
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Talking", meta = (ClampMin = "0"))
	float CombatRadius = 2500.0f;

	//~ Village chatter

	// NPCs chat among themselves while the leader is this close
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Chatter", meta = (ClampMin = "0"))
	float ChatterRadius = 1500.0f;

	// Seconds between two chats of the same NPC (random in the range)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Chatter")
	FVector2D ChatterInterval = FVector2D(20.0f, 40.0f);

	// First chat this long after the leader comes in range
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Chatter", meta = (ClampMin = "0"))
	float ChatterFirstDelay = 4.0f;

	// An NPC greets the leader once when it comes this close...
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Chatter", meta = (ClampMin = "0"))
	float GreetRadius = 450.0f;

	// ...and again only after this many seconds
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Chatter", meta = (ClampMin = "0"))
	float GreetCooldown = 120.0f;

	//~ Banter

	// Angel and Ji-Woong's lines (made by migrate_pass13.py)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banter")
	TSoftObjectPtr<UBeyondBanterSet> BanterSet;

	// Seconds between two banters (new regions and boss victories don't wait)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (ClampMin = "0"))
	float BanterCooldown = 45.0f;

	// Idle banter after this long without moving or fighting
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (ClampMin = "1"))
	float IdleSeconds = 40.0f;

	// Low-health banter when the leader drops under this share of max health in a fight
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float LowHealthFraction = 0.3f;

	// Seconds after a boss falls before the victory banter (its death plays first)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Banter", meta = (ClampMin = "0"))
	float BossBanterDelay = 3.0f;
};
