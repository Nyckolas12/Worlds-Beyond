// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BeyondAIAbilityUtils.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystemComponent.h"

namespace BeyondAI
{
	namespace
	{
		bool IsReady(UAbilitySystemComponent* ASC, const FGameplayAbilitySpec& Spec)
		{
			return !Spec.IsActive() && Spec.Ability->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get());
		}
	}

	bool SelectAbility(UAbilitySystemComponent* ASC, const AActor* Target, const AActor* Ally, FAbilityChoice& OutChoice)
	{
		if (!ASC || !ASC->AbilityActorInfo.IsValid())
		{
			return false;
		}

		const AActor* Self = ASC->GetAvatarActor();
		const float DistanceToTarget = (Self && Target) ? FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation()) : TNumericLimits<float>::Max();
		const float SelfHealth = UBeyondCombatLibrary::GetActorHealthPercent(Self);
		const float AllyHealth = Ally ? UBeyondCombatLibrary::GetActorHealthPercent(Ally) : 1.0f;
		const float LowestHealth = FMath::Min(SelfHealth, AllyHealth);

		struct FCandidate { FGameplayAbilitySpecHandle Handle; float Weight; bool bSelf; };
		TArray<FCandidate> Candidates;
		float TotalWeight = 0.0f;

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
			if (!Ability || !Ability->bAIUsable || Ability->bActivateOnGranted || Ability->AIWeight <= 0.0f)
			{
				continue;
			}

			if (LowestHealth > Ability->AIUseBelowHealthPercent)
			{
				continue;
			}

			const bool bSelf = Ability->AITargeting == EBeyondAITargeting::Self;
			if (!bSelf && (!Target || DistanceToTarget < Ability->AIMinRange || DistanceToTarget > Ability->AIMaxRange))
			{
				continue;
			}

			if (!IsReady(ASC, Spec))
			{
				continue;
			}

			Candidates.Add({ Spec.Handle, Ability->AIWeight, bSelf });
			TotalWeight += Ability->AIWeight;
		}

		if (Candidates.IsEmpty())
		{
			return false;
		}

		float Roll = FMath::FRandRange(0.0f, TotalWeight);
		for (const FCandidate& Candidate : Candidates)
		{
			Roll -= Candidate.Weight;
			if (Roll <= 0.0f)
			{
				OutChoice.Handle = Candidate.Handle;
				OutChoice.bTargetsSelf = Candidate.bSelf;
				return true;
			}
		}

		OutChoice.Handle = Candidates.Last().Handle;
		OutChoice.bTargetsSelf = Candidates.Last().bSelf;
		return true;
	}

	float GetPreferredEngageRange(const UAbilitySystemComponent* ASC)
	{
		float Range = 0.0f;
		if (!ASC)
		{
			return Range;
		}

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
			if (Ability && Ability->bAIUsable && !Ability->bActivateOnGranted && Ability->AITargeting == EBeyondAITargeting::Enemy)
			{
				Range = FMath::Max(Range, Ability->AIMaxRange);
			}
		}
		return Range;
	}

	bool IsUsingAbility(const UAbilitySystemComponent* ASC)
	{
		if (!ASC)
		{
			return false;
		}

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
			if (Spec.IsActive() && Ability && !Ability->bActivateOnGranted)
			{
				return true;
			}
		}
		return false;
	}
}
