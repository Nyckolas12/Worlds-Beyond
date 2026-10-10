// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemies/BeyondEnemyDefinition.h"
#include "Enemies/BeyondAffixDefinition.h"

UBeyondEnemyDefinition* UBeyondEnemyRoster::FindEnemy(FName EnemyId) const
{
	for (UBeyondEnemyDefinition* Enemy : Enemies)
	{
		if (Enemy && Enemy->EnemyId == EnemyId)
		{
			return Enemy;
		}
	}
	return nullptr;
}

UBeyondAffixDefinition* UBeyondEnemyRoster::FindAffix(FName AffixId) const
{
	for (UBeyondAffixDefinition* Affix : Affixes)
	{
		if (Affix && Affix->AffixId == AffixId)
		{
			return Affix;
		}
	}
	return nullptr;
}
