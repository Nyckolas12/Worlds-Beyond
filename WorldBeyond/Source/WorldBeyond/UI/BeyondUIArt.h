// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/BeyondElement.h"
#include "Engine/DataAsset.h"
#include "Items/BeyondCurrency.h"
#include "BeyondUIArt.generated.h"

class UTexture2D;

/**
 * The menus' and HUD's shared pictures (Plan 6): backdrops, emblems, glow / rune textures and the icons that aren't on
 * an item, skill node or ability. Made by migrate_pass20.py, filled by migrate_pass21.py (the generated art); anything
 * left empty is drawn in code instead. Project Settings -> Game -> Worlds Beyond UI -> Art Set picks the asset.
 */
UCLASS(BlueprintType)
class WORLDBEYOND_API UBeyondUIArtSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UBeyondUIArtSet();

	// Behind the character menu, the map and the pause menu (under a navy veil)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrops")
	TSoftObjectPtr<UTexture2D> MenuBackdrop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrops")
	TSoftObjectPtr<UTexture2D> MainMenuBackdrop;

	// Soft round light for glows (white in the middle, clear at the edge)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shapes")
	TSoftObjectPtr<UTexture2D> Glow;

	// A rune ring laid faintly behind the paper doll and the skill graph
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shapes")
	TSoftObjectPtr<UTexture2D> RuneCircle;

	// The compass star beside "CHARACTER"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emblems")
	TSoftObjectPtr<UTexture2D> CharacterEmblem;

	// The crescent on Bond Status
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emblems")
	TSoftObjectPtr<UTexture2D> BondEmblem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TMap<EBeyondCurrency, TSoftObjectPtr<UTexture2D>> CurrencyIcons;

	// Skill Points and Bond Points (the trees' currencies)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TSoftObjectPtr<UTexture2D> SkillPointIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TSoftObjectPtr<UTexture2D> BondPointIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TMap<EBeyondElement, TSoftObjectPtr<UTexture2D>> ElementIcons;

	// Icons for abilities that have none of their own (Blueprint abilities such as GA_HealSpell), by class name ("GA_HealSpell_C")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TMap<FName, TSoftObjectPtr<UTexture2D>> AbilityIcons;
};
