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
		}
		return FGameplayTag();
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

		static const EBeyondSkillStat AllStats[] = { EBeyondSkillStat::MaxHealth, EBeyondSkillStat::MaxStamina, EBeyondSkillStat::Strength,
			EBeyondSkillStat::Arcana, EBeyondSkillStat::Defense };
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
