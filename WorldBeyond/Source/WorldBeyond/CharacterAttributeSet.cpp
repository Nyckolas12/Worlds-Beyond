// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterAttributeSet.h"
#include "BeyondGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// First Event.Hit.* tag on the spec, used to pick the hit reaction
	FGameplayTag FindHitResponse(const FGameplayEffectSpec& Spec)
	{
		FGameplayTagContainer AssetTags;
		Spec.GetAllAssetTags(AssetTags);
		for (const FGameplayTag& Tag : AssetTags)
		{
			if (Tag.MatchesTag(BeyondTags::Event_Hit) && Tag != BeyondTags::Event_Hit)
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	bool SpecHasTag(const FGameplayEffectSpec& Spec, const FGameplayTag& Tag)
	{
		FGameplayTagContainer AssetTags;
		Spec.GetAllAssetTags(AssetTags);
		return AssetTags.HasTagExact(Tag);
	}

	void SendHitEvent(UAbilitySystemComponent* ASC, const FGameplayTag& EventTag, AActor* Instigator, float Magnitude)
	{
		if (!ASC || !EventTag.IsValid())
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = Instigator;
		Payload.Target = ASC->GetAvatarActor();
		Payload.EventMagnitude = Magnitude;
		ASC->HandleGameplayEvent(EventTag, &Payload);
	}
}

UCharacterAttributeSet::UCharacterAttributeSet():
CurrentHealth(100.f),
MaxHealth(100.f),
CurrentStamina(100.f),
MaxStamina(100.f),
IncomingDamage(0.f),
IncomingHeal(0.f)
{
}

void UCharacterAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetCurrentHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetCurrentStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UCharacterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UCharacterAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// Lowering a max must pull the current value down with it
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (Attribute == GetMaxHealthAttribute() && GetCurrentHealth() > NewValue)
	{
		ASC->ApplyModToAttribute(GetCurrentHealthAttribute(), EGameplayModOp::Override, NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetCurrentStamina() > NewValue)
	{
		ASC->ApplyModToAttribute(GetCurrentStaminaAttribute(), EGameplayModOp::Override, NewValue);
	}
}

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;

	if (Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Data);
	}
	else if (Attribute == GetIncomingHealAttribute())
	{
		HandleIncomingHeal(Data);
	}
	else if (Attribute == GetCurrentHealthAttribute())
	{
		// Legacy effects (e.g. GE_Damage_Instant) that modify CurrentHealth directly
		SetCurrentHealth(FMath::Clamp(GetCurrentHealth(), 0.0f, GetMaxHealth()));

		if (Data.EvaluatedData.Magnitude < 0.0f)
		{
			const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
			OnHitTaken.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), -Data.EvaluatedData.Magnitude, BeyondTags::Event_Hit_Light);
		}
		CheckOutOfHealth(Data);
	}
	else if (Attribute == GetCurrentStaminaAttribute())
	{
		SetCurrentStamina(FMath::Clamp(GetCurrentStamina(), 0.0f, GetMaxStamina()));
	}
}

void UCharacterAttributeSet::HandleIncomingDamage(const FGameplayEffectModCallbackData& Data)
{
	float Damage = GetIncomingDamage();
	SetIncomingDamage(0.0f);

	UAbilitySystemComponent& TargetASC = Data.Target;
	const FGameplayEffectSpec& Spec = Data.EffectSpec;
	const FGameplayEffectContextHandle& Context = Spec.GetEffectContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	if (Damage <= 0.0f || TargetASC.HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		return;
	}

	if (TargetASC.HasMatchingGameplayTag(BeyondTags::State_Invincible) && !SpecHasTag(Spec, BeyondTags::Damage_IgnoreInvincible))
	{
		return;
	}

	// Parry: no damage, and the attacker gets staggered
	if (TargetASC.HasMatchingGameplayTag(BeyondTags::State_Parrying) && !SpecHasTag(Spec, BeyondTags::Damage_Unparryable))
	{
		SendHitEvent(&TargetASC, BeyondTags::Event_Hit_Parried, Instigator, 0.0f);
		SendHitEvent(Context.GetOriginalInstigatorAbilitySystemComponent(), BeyondTags::Event_Hit_Stagger, TargetASC.GetAvatarActor(), 0.0f);
		OnHitTaken.Broadcast(Instigator, Causer, 0.0f, BeyondTags::Event_Hit_Parried);
		return;
	}

	// Block: no damage (matches the old BPC_DamageSystem behaviour)
	if (TargetASC.HasMatchingGameplayTag(BeyondTags::State_Blocking) && !SpecHasTag(Spec, BeyondTags::Damage_Unblockable))
	{
		SendHitEvent(&TargetASC, BeyondTags::Event_Hit_Blocked, Instigator, 0.0f);
		OnHitTaken.Broadcast(Instigator, Causer, 0.0f, BeyondTags::Event_Hit_Blocked);
		return;
	}

	SetCurrentHealth(FMath::Clamp(GetCurrentHealth() - Damage, 0.0f, GetMaxHealth()));

	FGameplayTag HitResponse = FindHitResponse(Spec);
	if (TargetASC.HasMatchingGameplayTag(BeyondTags::State_Uninterruptible) && !SpecHasTag(Spec, BeyondTags::Damage_ForceInterrupt))
	{
		HitResponse = FGameplayTag();
	}

	SendHitEvent(&TargetASC, HitResponse, Instigator, Damage);
	OnHitTaken.Broadcast(Instigator, Causer, Damage, HitResponse);

	CheckOutOfHealth(Data);
}

void UCharacterAttributeSet::HandleIncomingHeal(const FGameplayEffectModCallbackData& Data)
{
	const float Heal = GetIncomingHeal();
	SetIncomingHeal(0.0f);

	if (Heal <= 0.0f || Data.Target.HasMatchingGameplayTag(BeyondTags::State_Dead))
	{
		return;
	}

	SetCurrentHealth(FMath::Clamp(GetCurrentHealth() + Heal, 0.0f, GetMaxHealth()));
	CheckOutOfHealth(Data);
}

void UCharacterAttributeSet::CheckOutOfHealth(const FGameplayEffectModCallbackData& Data)
{
	if (GetCurrentHealth() > 0.0f)
	{
		// Healed or revived, allow death to fire again
		bOutOfHealth = false;
		return;
	}

	if (!bOutOfHealth)
	{
		bOutOfHealth = true;
		const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
		OnOutOfHealth.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), Data.EvaluatedData.Magnitude, FGameplayTag());
	}
}

void UCharacterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, CurrentHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, CurrentStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}
