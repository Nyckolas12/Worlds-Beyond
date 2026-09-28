// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "CharacterAttributeSet.h"
#include "GenericTeamAgentInterface.h"

UAbilitySystemComponent* UBeyondCombatLibrary::GetASC(const AActor* Actor)
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Actor));
}

bool UBeyondCombatLibrary::ApplyDamage(AActor* Source, AActor* Target, float Amount, FGameplayTag DamageType, FGameplayTag HitResponse,
	bool bUnblockable, AActor* Causer, bool bUnparryable, bool bIgnoreInvincible, bool bForceInterrupt)
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	if (!TargetASC || Amount <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetASC(Source);
	UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
	Context.AddInstigator(Source, Causer ? Causer : Source);

	const FGameplayEffectSpecHandle SpecHandle = SpecOwner->MakeOutgoingSpec(UBeyondGE_Damage::StaticClass(), 1.0f, Context);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return false;
	}

	Spec->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Damage, Amount);
	if (DamageType.IsValid()) { Spec->AddDynamicAssetTag(DamageType); }
	if (HitResponse.IsValid()) { Spec->AddDynamicAssetTag(HitResponse); }
	if (bUnblockable) { Spec->AddDynamicAssetTag(BeyondTags::Damage_Unblockable); }
	if (bUnparryable) { Spec->AddDynamicAssetTag(BeyondTags::Damage_Unparryable); }
	if (bIgnoreInvincible) { Spec->AddDynamicAssetTag(BeyondTags::Damage_IgnoreInvincible); }
	if (bForceInterrupt) { Spec->AddDynamicAssetTag(BeyondTags::Damage_ForceInterrupt); }

	if (SourceASC)
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
	}
	else
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);
	}
	return true;
}

bool UBeyondCombatLibrary::ApplyLegacyDamage(AActor* Source, AActor* Target, float Amount, uint8 DamageType, uint8 DamageResponse,
	bool bShouldDamageInvincible, bool bCanBeBlocked, bool bCanBeParried, bool bShouldForceInterrupt)
{
	return ApplyDamage(Source, Target, Amount, DamageTypeFromLegacy(DamageType), HitResponseFromLegacy(DamageResponse),
		!bCanBeBlocked, Source, !bCanBeParried, bShouldDamageInvincible, bShouldForceInterrupt);
}

bool UBeyondCombatLibrary::ApplyHeal(AActor* Source, AActor* Target, float Amount)
{
	UAbilitySystemComponent* TargetASC = GetASC(Target);
	if (!TargetASC || Amount <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetASC(Source);
	UAbilitySystemComponent* SpecOwner = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecOwner->MakeEffectContext();
	Context.AddInstigator(Source, Source);

	const FGameplayEffectSpecHandle SpecHandle = SpecOwner->MakeOutgoingSpec(UBeyondGE_Heal::StaticClass(), 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Heal, Amount);
	SpecOwner->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

bool UBeyondCombatLibrary::IsActorDead(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetASC(Actor);
	return ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Dead);
}

float UBeyondCombatLibrary::GetActorHealth(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetASC(Actor);
	return ASC ? ASC->GetNumericAttribute(UCharacterAttributeSet::GetCurrentHealthAttribute()) : 0.0f;
}

float UBeyondCombatLibrary::GetActorMaxHealth(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetASC(Actor);
	return ASC ? ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()) : 0.0f;
}

float UBeyondCombatLibrary::GetActorHealthPercent(const AActor* Actor)
{
	const float Max = GetActorMaxHealth(Actor);
	return Max > 0.0f ? GetActorHealth(Actor) / Max : 0.0f;
}

bool UBeyondCombatLibrary::AreHostile(const AActor* A, const AActor* B)
{
	const FGenericTeamId TeamA = FGenericTeamId::GetTeamIdentifier(A);
	const FGenericTeamId TeamB = FGenericTeamId::GetTeamIdentifier(B);

	// Props and other actors without a team are never hostile
	if (TeamA == FGenericTeamId::NoTeam || TeamB == FGenericTeamId::NoTeam)
	{
		return false;
	}
	return FGenericTeamId::GetAttitude(TeamA, TeamB) == ETeamAttitude::Hostile;
}

void UBeyondCombatLibrary::SetInvincible(AActor* Actor, bool bInvincible)
{
	SetStateTag(Actor, BeyondTags::State_Invincible, bInvincible);
}

void UBeyondCombatLibrary::SetUninterruptible(AActor* Actor, bool bUninterruptible)
{
	SetStateTag(Actor, BeyondTags::State_Uninterruptible, bUninterruptible);
}

void UBeyondCombatLibrary::SetStateTag(AActor* Actor, FGameplayTag StateTag, bool bActive)
{
	if (UAbilitySystemComponent* ASC = GetASC(Actor))
	{
		ASC->SetLooseGameplayTagCount(StateTag, bActive ? 1 : 0);
	}
}

FGameplayTag UBeyondCombatLibrary::DamageTypeFromLegacy(uint8 DamageType)
{
	switch (DamageType)
	{
	case 1: return BeyondTags::DamageType_Melee;
	case 2: return BeyondTags::DamageType_Projectile;
	case 3: return BeyondTags::DamageType_Explosion;
	case 4: return BeyondTags::DamageType_Environment;
	default: return FGameplayTag();
	}
}

FGameplayTag UBeyondCombatLibrary::HitResponseFromLegacy(uint8 DamageResponse)
{
	switch (DamageResponse)
	{
	case 1: return BeyondTags::Event_Hit_Light;
	case 2: return BeyondTags::Event_Hit_Stagger;
	case 3: return BeyondTags::Event_Hit_Stun;
	case 4: return BeyondTags::Event_Hit_KnockBack;
	default: return FGameplayTag();
	}
}
