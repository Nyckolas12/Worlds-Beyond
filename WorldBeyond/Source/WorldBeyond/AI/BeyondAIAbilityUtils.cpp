// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BeyondAIAbilityUtils.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystemComponent.h"

namespace BeyondAI
{
	namespace
	{
		bool IsReady(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpec& Spec)
		{
			const UGameplayAbility* Source = Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
			return !Spec.IsActive() && Source->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get());
		}
	}

	bool SelectAbility(UAbilitySystemComponent* ASC, const AActor* Target, const AActor* Ally, FAbilityChoice& OutChoice)
	{
		return SelectAbility(ASC, Target, Ally, OutChoice, [](const UBeyondGameplayAbility&) { return true; });
	}

	bool SelectAbility(UAbilitySystemComponent* ASC, const AActor* Target, const AActor* Ally, FAbilityChoice& OutChoice,
		TFunctionRef<bool(const UBeyondGameplayAbility&)> Filter)
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
			if (!Ability || !Ability->bAIUsable || Ability->bActivateOnGranted || Ability->AIWeight <= 0.0f || !Filter(*Ability))
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
		if (!ASC || !ASC->AbilityActorInfo.IsValid())
		{
			return 0.0f;
		}

		// Stand where the longest-reaching ready ability works; with everything on cooldown, close in
		// to the shortest reach (e.g. Ji-Woong walks up to sword range while Sunbrand recharges)
		float ReadyRange = 0.0f;
		float ShortestRange = TNumericLimits<float>::Max();
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
			if (Ability && Ability->bAIUsable && !Ability->bActivateOnGranted && Ability->AITargeting == EBeyondAITargeting::Enemy)
			{
				ShortestRange = FMath::Min(ShortestRange, Ability->AIMaxRange);
				if (IsReady(ASC, Spec))
				{
					ReadyRange = FMath::Max(ReadyRange, Ability->AIMaxRange);
				}
			}
		}
		if (ReadyRange > 0.0f)
		{
			return ReadyRange;
		}
		return ShortestRange < TNumericLimits<float>::Max() ? ShortestRange : 0.0f;
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
