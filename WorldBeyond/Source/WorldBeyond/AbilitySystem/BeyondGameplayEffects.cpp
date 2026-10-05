// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondGameplayEffects.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"

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
