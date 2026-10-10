// Fill out your copyright notice in the Description page of Project Settings.

#include "Dialogue/BeyondDialogueSettings.h"
#include "Dialogue/BeyondBanter.h"
#include "Engine/DataTable.h"

UBeyondDialogueSettings::UBeyondDialogueSettings()
{
	// Where the pack is installed; DefaultGame.ini can point elsewhere
	DialogueComponentClass = TSoftClassPtr<UActorComponent>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/ActorComponent/BP_AC_Dialogue.BP_AC_Dialogue_C")));
	OverHeadComponentClass = TSoftClassPtr<UActorComponent>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/ActorComponent/BP_AC_DialogueOverHead.BP_AC_DialogueOverHead_C")));
	OverHeadWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/Widget/WBP_TextOverHead.WBP_TextOverHead_C")));
	FocusCameraClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/Blueprint/BP_CameraActor.BP_CameraActor_C")));
	DialogueTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/DataTable/DT_Dialogue.DT_Dialogue")));
	OverHeadTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/DataTable/DT_TextOverHead.DT_TextOverHead")));
	SpeakersTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/DialogueSystem/Blueprint/DataTable/DT_Speakers.DT_Speakers")));
	BanterSet = TSoftObjectPtr<UBeyondBanterSet>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Dialogue/DA_Banter.DA_Banter")));
}
