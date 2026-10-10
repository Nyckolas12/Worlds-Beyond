// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemySettings.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Materials/MaterialInterface.h"

UBeyondEnemySettings::UBeyondEnemySettings()
{
	// The assets migrate_pass10.py makes
	Roster = TSoftObjectPtr<UBeyondEnemyRoster>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Enemies/DA_EnemyRoster.DA_EnemyRoster")));
	TelegraphMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Enemies/Materials/M_Beyond_Telegraph.M_Beyond_Telegraph")));
	TintMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/WorldsBeyond/Enemies/Materials/M_Beyond_EnemyTint.M_Beyond_EnemyTint")));
}

UBeyondEnemyRoster* UBeyondEnemySettings::GetRoster()
{
	return GetDefault<UBeyondEnemySettings>()->Roster.LoadSynchronous();
}
