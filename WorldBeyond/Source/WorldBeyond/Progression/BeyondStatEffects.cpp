// Fill out your copyright notice in the Description page of Project Settings.

#include "Progression/BeyondStatEffects.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "GameplayEffect.h"

namespace BeyondStats
{
	FGameplayTag GetStatTag(EBeyondSkillStat Stat)
	{
		switch (Stat)
		{
		case EBeyondSkillStat::MaxHealth: return BeyondTags::SetByCaller_MaxHealth;
		case EBeyondSkillStat::MaxStamina: return BeyondTags::SetByCaller_MaxStamina;
		case EBeyondSkillStat::Strength: return BeyondTags::SetByCaller_Strength;
		case EBeyondSkillStat::Arcana: return BeyondTags::SetByCaller_Arcana;
		case EBeyondSkillStat::Defense: return BeyondTags::SetByCaller_Defense;
		case EBeyondSkillStat::CritRate: return BeyondTags::SetByCaller_CritRate;
		case EBeyondSkillStat::CritDamage: return BeyondTags::SetByCaller_CritDamage;
		case EBeyondSkillStat::ElementalMastery: return BeyondTags::SetByCaller_ElementalMastery;
		case EBeyondSkillStat::VerdantAffinity: return BeyondTags::SetByCaller_VerdantAffinity;
		case EBeyondSkillStat::ShadowAffinity: return BeyondTags::SetByCaller_ShadowAffinity;
		case EBeyondSkillStat::FrostAffinity: return BeyondTags::SetByCaller_FrostAffinity;
		case EBeyondSkillStat::FlameAffinity: return BeyondTags::SetByCaller_FlameAffinity;
		}
		return FGameplayTag();
	}

	FGameplayAttribute GetStatAttribute(EBeyondSkillStat Stat)
	{
		switch (Stat)
		{
		case EBeyondSkillStat::MaxHealth: return UCharacterAttributeSet::GetMaxHealthAttribute();
		case EBeyondSkillStat::MaxStamina: return UCharacterAttributeSet::GetMaxStaminaAttribute();
		case EBeyondSkillStat::Strength: return UCharacterAttributeSet::GetStrengthAttribute();
		case EBeyondSkillStat::Arcana: return UCharacterAttributeSet::GetArcanaAttribute();
		case EBeyondSkillStat::Defense: return UCharacterAttributeSet::GetDefenseAttribute();
		case EBeyondSkillStat::CritRate: return UCharacterAttributeSet::GetCritRateAttribute();
		case EBeyondSkillStat::CritDamage: return UCharacterAttributeSet::GetCritDamageAttribute();
		case EBeyondSkillStat::ElementalMastery: return UCharacterAttributeSet::GetElementalMasteryAttribute();
		case EBeyondSkillStat::VerdantAffinity: return UCharacterAttributeSet::GetVerdantAffinityAttribute();
		case EBeyondSkillStat::ShadowAffinity: return UCharacterAttributeSet::GetShadowAffinityAttribute();
		case EBeyondSkillStat::FrostAffinity: return UCharacterAttributeSet::GetFrostAffinityAttribute();
		case EBeyondSkillStat::FlameAffinity: return UCharacterAttributeSet::GetFlameAffinityAttribute();
		}
		return FGameplayAttribute();
	}

	const TArray<EBeyondSkillStat>& GetAllStats()
	{
		static const TArray<EBeyondSkillStat> Stats = { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength,
			EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense, EBeyondSkillStat::CritRate, EBeyondSkillStat::CritDamage, EBeyondSkillStat::ElementalMastery,
			EBeyondSkillStat::VerdantAffinity, EBeyondSkillStat::ShadowAffinity, EBeyondSkillStat::FrostAffinity, EBeyondSkillStat::FlameAffinity };
		return Stats;
	}

	bool IsPercentStat(EBeyondSkillStat Stat)
	{
		return Stat == EBeyondSkillStat::CritRate || Stat == EBeyondSkillStat::CritDamage || Stat == EBeyondSkillStat::VerdantAffinity
			|| Stat == EBeyondSkillStat::ShadowAffinity || Stat == EBeyondSkillStat::FrostAffinity || Stat == EBeyondSkillStat::FlameAffinity;
	}

	FGameplayTag GetElementTag(EBeyondElement Element)
	{
		switch (Element)
		{
		case EBeyondElement::Storm: return BeyondTags::Element_Storm;
		case EBeyondElement::Radiant: return BeyondTags::Element_Radiant;
		case EBeyondElement::Eclipse: return BeyondTags::Element_Eclipse;
		case EBeyondElement::Verdant: return BeyondTags::Element_Verdant;
		case EBeyondElement::Shadow: return BeyondTags::Element_Shadow;
		case EBeyondElement::Frost: return BeyondTags::Element_Frost;
		case EBeyondElement::Flame: return BeyondTags::Element_Flame;
		default: return FGameplayTag();
		}
	}

	EBeyondElement GetElementFromTags(const FGameplayTagContainer& Tags)
	{
		for (const EBeyondElement Element : { EBeyondElement::Storm, EBeyondElement::Radiant, EBeyondElement::Eclipse, EBeyondElement::Verdant,
			EBeyondElement::Shadow, EBeyondElement::Frost, EBeyondElement::Flame })
		{
			if (Tags.HasTagExact(GetElementTag(Element)))
			{
				return Element;
			}
		}
		return EBeyondElement::None;
	}

	bool GetAffinityStat(EBeyondElement Element, EBeyondSkillStat& OutStat)
	{
		switch (Element)
		{
		case EBeyondElement::Verdant: OutStat = EBeyondSkillStat::VerdantAffinity; return true;
		case EBeyondElement::Shadow: OutStat = EBeyondSkillStat::ShadowAffinity; return true;
		case EBeyondElement::Frost: OutStat = EBeyondSkillStat::FrostAffinity; return true;
		case EBeyondElement::Flame: OutStat = EBeyondSkillStat::FlameAffinity; return true;
		default: return false;
		}
	}

	FActiveGameplayEffectHandle ApplyStatBonuses(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass,
		const TMap<EBeyondSkillStat, float>& Bonuses, FActiveGameplayEffectHandle OldHandle)
	{
		if (!ASC || !EffectClass)
		{
			return FActiveGameplayEffectHandle();
		}

		const float OldMaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
		const float OldMaxStamina = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxStaminaAttribute());

		const TArray<EBeyondSkillStat>& AllStats = GetAllStats();
		bool bAnyStat = false;
		for (const TPair<EBeyondSkillStat, float>& Bonus : Bonuses)
		{
			bAnyStat |= !FMath::IsNearlyZero(Bonus.Value);
		}

		FActiveGameplayEffectHandle NewHandle;
		if (bAnyStat)
		{
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				for (const EBeyondSkillStat Stat : AllStats)
				{
					Spec.Data->SetSetByCallerMagnitude(GetStatTag(Stat), Bonuses.FindRef(Stat));
				}
				NewHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			}
		}
		if (OldHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(OldHandle);
		}

		// A bigger maximum comes with the extra health / stamina (taking it back is the clamp's job)
		if (!ASC->HasMatchingGameplayTag(BeyondTags::State_Dead))
		{
			const float HealthGain = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()) - OldMaxHealth;
			if (HealthGain > 0.0f)
			{
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute(),
					ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetCurrentHealthAttribute()) + HealthGain);
			}
			const float StaminaGain = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxStaminaAttribute()) - OldMaxStamina;
			if (StaminaGain > 0.0f)
			{
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute(),
					ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetCurrentStaminaAttribute()) + StaminaGain);
			}
		}
		return NewHandle;
	}
}
