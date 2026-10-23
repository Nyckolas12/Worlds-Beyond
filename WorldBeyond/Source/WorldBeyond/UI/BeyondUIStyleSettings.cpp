// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BeyondUIStyleSettings.h"
#include "Engine/Font.h"
#include "Sound/SoundBase.h"
#include "UI/BeyondUIArt.h"

UBeyondUIStyleSettings::UBeyondUIStyleSettings()
{
	// Made by migrate_pass20.py (set here so no config has to be written)
	HeadingFont = TSoftObjectPtr<UFont>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/UI/Fonts/F_Cinzel.F_Cinzel")));
	BodyFont = TSoftObjectPtr<UFont>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/UI/Fonts/F_EBGaramond.F_EBGaramond")));
	ArtSet = TSoftObjectPtr<UBeyondUIArtSet>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/UI/DA_UIArt.DA_UIArt")));
	// The plain sound waves (the UI_*_MS MetaSounds next to them assert in MetaSound versioning when loaded)
	HoverSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/_Content/SFX/UI/wav/UI_Menu_Hover_01.UI_Menu_Hover_01")));
	SelectSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/_Content/SFX/UI/wav/UI_Menu_Select.UI_Menu_Select")));
	PageSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/_Content/SFX/UI/wav/UI_Menu_Setting_Change.UI_Menu_Setting_Change")));
}
