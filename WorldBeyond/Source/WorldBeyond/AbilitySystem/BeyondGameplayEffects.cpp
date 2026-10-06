// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondGameplayEffects.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "Progression/BeyondProgressionAttributeSet.h"

namespace
{
	FGameplayModifierInfo MakeSetByCallerModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::AddBase;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}
}

UBeyondGE_Damage::UBeyondGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetIncomingDamageAttribute(), BeyondTags::SetByCaller_Damage));
	GameplayCues.Add(FGameplayEffectCue(BeyondTags::GameplayCue_Damage_Burst, 0.0f, 1.0f));
}

UBeyondGE_Heal::UBeyondGE_Heal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetIncomingHealAttribute(), BeyondTags::SetByCaller_Heal));
	GameplayCues.Add(FGameplayEffectCue(BeyondTags::GameplayCue_Heal_Burst, 0.0f, 1.0f));
}

UBeyondGE_GrantExperience::UBeyondGE_GrantExperience()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerModifier(UBeyondProgressionAttributeSet::GetIncomingExperienceAttribute(), BeyondTags::SetByCaller_Experience));
}

UBeyondGE_LevelStats::UBeyondGE_LevelStats()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetMaxHealthAttribute(), BeyondTags::SetByCaller_MaxHealth));
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetMaxStaminaAttribute(), BeyondTags::SetByCaller_MaxStamina));
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetStrengthAttribute(), BeyondTags::SetByCaller_Strength));
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetArcanaAttribute(), BeyondTags::SetByCaller_Arcana));
	Modifiers.Add(MakeSetByCallerModifier(UCharacterAttributeSet::GetDefenseAttribute(), BeyondTags::SetByCaller_Defense));
}

namespace
{
	FGameplayEffectModifierMagnitude SetByCallerDuration()
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = BeyondTags::SetByCaller_Duration;
		return FGameplayEffectModifierMagnitude(SetByCaller);
	}
}

UBeyondGE_Cooldown::UBeyondGE_Cooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = SetByCallerDuration();
}

UBeyondGE_Brand::UBeyondGE_Brand()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = SetByCallerDuration();
}
