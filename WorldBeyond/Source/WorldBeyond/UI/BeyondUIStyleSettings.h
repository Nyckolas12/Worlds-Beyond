// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BeyondUIStyleSettings.generated.h"

class UBeyondUIArtSet;
class UFont;
class USoundBase;

/**
 * The look shared by every menu and HUD piece (Project Settings -> Game -> Worlds Beyond UI, Plan 6): the two font
 * families, the art set and the UI sounds. Defaults point at the assets migrate_pass20.py makes; anything missing falls
 * back to the engine font and drawn shapes.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond UI"))
class WORLDBEYOND_API UBeyondUIStyleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBeyondUIStyleSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UBeyondUIStyleSettings* Get() { return GetDefault<UBeyondUIStyleSettings>(); }

	// Titles, headings, tabs and numbers: Cinzel (typefaces "Regular", "Bold")
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Fonts")
	TSoftObjectPtr<UFont> HeadingFont;

	// Running text: EB Garamond (typefaces "Regular", "Italic", "Bold")
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Fonts")
	TSoftObjectPtr<UFont> BodyFont;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UBeyondUIArtSet> ArtSet;

	// The menus are laid out at this size and scaled uniformly to the screen
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FVector2D ReferenceResolution = FVector2D(1920.0, 1080.0);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> HoverSound;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> SelectSound;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> PageSound;
};
