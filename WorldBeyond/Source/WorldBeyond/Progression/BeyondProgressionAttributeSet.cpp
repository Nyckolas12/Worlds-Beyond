// Fill out your copyright notice in the Description page of Project Settings.

#include "Progression/BeyondProgressionAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Progression/BeyondProgressionSettings.h"

UBeyondProgressionAttributeSet::UBeyondProgressionAttributeSet()
	: Experience(0.0f)
	, SkillPoints(0.0f)
	, IncomingExperience(0.0f)
{
}

void UBeyondProgressionAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute != GetIncomingExperienceAttribute())
	{
		return;
	}

	const float Gained = GetIncomingExperience();
	SetIncomingExperience(0.0f);
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (Gained <= 0.0f || !ASC)
	{
		return;
	}

	const UBeyondProgressionSettings* Settings = GetDefault<UBeyondProgressionSettings>();
	const FGameplayAttribute LevelAttribute = UCharacterAttributeSet::GetLevelAttribute();
	const int32 OldLevel = ASC->HasAttributeSetForAttribute(LevelAttribute) ? FMath::Max(1, FMath::RoundToInt(ASC->GetNumericAttributeBase(LevelAttribute))) : 1;

	int32 NewLevel = OldLevel;
	float NewExperience = GetExperience() + Gained;
	float NewSkillPoints = GetSkillPoints();
	while (NewLevel < Settings->MaxLevel)
	{
		const float Needed = UBeyondProgressionSettings::GetExperienceToNextLevel(NewLevel);
		if (NewExperience < Needed)
		{
			break;
		}
		NewExperience -= Needed;
		++NewLevel;
		NewSkillPoints += Settings->SkillPointsPerLevel;
	}
	if (NewLevel >= Settings->MaxLevel)
	{
		NewExperience = 0.0f;
	}

	SetExperience(NewExperience);
	SetSkillPoints(NewSkillPoints);
	if (NewLevel != OldLevel)
	{
		ASC->SetNumericAttributeBase(LevelAttribute, static_cast<float>(NewLevel));
		OnLevelUp.Broadcast(OldLevel, NewLevel);
	}
	OnExperienceChanged.Broadcast();
}

void UBeyondProgressionAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBeyondProgressionAttributeSet, Experience, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBeyondProgressionAttributeSet, SkillPoints, COND_None, REPNOTIFY_Always);
}
