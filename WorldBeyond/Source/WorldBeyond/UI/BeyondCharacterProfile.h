// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondElement.h"
#include "Engine/DataAsset.h"
#include "BeyondCharacterProfile.generated.h"

class UTexture2D;

/**
 * How a demigod (or the duo) is presented on the menus (Plan 6): class title and motto, element, accent colour and the
 * portraits. Set as Profile on BP_Angel / BP_Ji-Woong and as Duo Profile on BP_PC (migrate_pass20.py).
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondCharacterProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// "Stormcaller"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FText ClassTitle;

	// "Command the elements. Control the storm."
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FText Motto;

	// The element Elemental Mastery boosts (the centre of the affinity diamond)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	EBeyondElement PrimaryElement = EBeyondElement::None;

	// Selected tabs, lit skill nodes, bars
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	FLinearColor AccentColor = FLinearColor(0.05f, 0.25f, 1.0f);

	// Class emblem beside the name
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> Emblem;

	// The tall party card on the Overview (3:4)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> CardPortrait;

	// The large figure beside the skill tree (back view, 2:3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> FullArt;

	// The dark figure inside the Armor screen's rune ring
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> Silhouette;

	// The round face on the HUD's vitals
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> Medallion;
};
