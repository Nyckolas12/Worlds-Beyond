// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondAbilitySet.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

void UBeyondAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, FBeyondAbilitySetHandles* OutHandles) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FBeyondAbilitySet_Ability& Entry : Abilities)
	{
		if (!Entry.Ability)
		{
			continue;
		}

		FGameplayAbilitySpec Spec(Entry.Ability, Entry.Level, INDEX_NONE, SourceObject);

		FGameplayTag InputTag = Entry.InputTag;
		if (!InputTag.IsValid())
		{
			if (const UBeyondGameplayAbility* BeyondCDO = Cast<UBeyondGameplayAbility>(Entry.Ability->GetDefaultObject()))
			{
				InputTag = BeyondCDO->InputTag;
			}
		}
		if (InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (OutHandles)
		{
			OutHandles->AbilityHandles.Add(Handle);
		}
	}

	for (const FBeyondAbilitySet_Effect& Entry : Effects)
	{
		if (!Entry.Effect)
		{
			continue;
		}

		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(Entry.Effect->GetDefaultObject<UGameplayEffect>(), Entry.Level, ASC->MakeEffectContext());
		if (OutHandles)
		{
			OutHandles->EffectHandles.Add(Handle);
		}
	}
}
