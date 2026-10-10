// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BeyondEnemySettings.generated.h"

class UBeyondEnemyRoster;
class UMaterialInterface;

/**
 * Enemies (Project Settings -> Game -> Worlds Beyond Enemies): the roster, what makes an elite, how attack telegraphs
 * look and the health plates. Saved in Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Worlds Beyond Enemies"))
class WORLDBEYOND_API UBeyondEnemySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBeyondEnemySettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Every enemy and affix (made by migrate_pass10.py)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Roster")
	TSoftObjectPtr<UBeyondEnemyRoster> Roster;

	//~ Elites

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Elites", meta = (ClampMin = "1"))
	float EliteHealthMultiplier = 1.8f;

	// Added to Strength and Arcana (+1 % damage per point)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Elites", meta = (ClampMin = "0"))
	float EliteDamageBonus = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Elites", meta = (ClampMin = "1"))
	float EliteScale = 1.12f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Elites", meta = (ClampMin = "0"))
	int32 AffixesPerElite = 1;

	// From this enemy level on, elites roll one more affix
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Elites", meta = (ClampMin = "1"))
	int32 ExtraAffixFromLevel = 10;

	//~ Telegraphs

	// Decal material for attack telegraphs (M_Beyond_Telegraph: Shape, Fill, Inner, HalfAngle, Color, ForwardSign)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Telegraphs")
	TSoftObjectPtr<UMaterialInterface> TelegraphMaterial;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Telegraphs")
	FLinearColor TelegraphColor = FLinearColor(1.0f, 0.28f, 0.08f, 1.0f);

	// Lingering hazards (lava, poison pools)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Telegraphs")
	FLinearColor HazardColor = FLinearColor(1.0f, 0.45f, 0.05f, 1.0f);

	// Fresnel overlay used for region / affix tints (M_Beyond_EnemyTint, vector parameter Color)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TSoftObjectPtr<UMaterialInterface> TintMaterial;

	//~ Health plates

	// Plates show over enemies in combat within this distance of the camera
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Plates", meta = (ClampMin = "0"))
	float PlateRange = 2800.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Plates", meta = (ClampMin = "0"))
	float PlateShowAfterHit = 6.0f;

	UFUNCTION(BlueprintPure, Category = "Beyond|Enemies")
	static UBeyondEnemyRoster* GetRoster();
};
