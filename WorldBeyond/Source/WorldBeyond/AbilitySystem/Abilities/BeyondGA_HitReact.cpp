// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_HitReact.h"
#include "AbilitySystemComponent.h"
#include "AI/BeyondAIAbilityUtils.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Enemies/BeyondEnemyCharacter.h"
#include "Enemies/BeyondEnemyDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UBeyondGA_HitReact::UBeyondGA_HitReact()
{
	bAIUsable = false;
	bRetriggerInstancedAbility = true;
	SetAssetTags(FGameplayTagContainer(BeyondTags::Ability_HitReact));

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = BeyondTags::Event_Hit;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

bool UBeyondGA_HitReact::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (!Payload || !Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const FGameplayTag& Response = Payload->EventTag;
	if (Response == BeyondTags::Event_Hit_Blocked || Response == BeyondTags::Event_Hit_Parried || Response == BeyondTags::Event_Hit)
	{
		return false;
	}

	const ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Enemy || !ASC || ASC->HasMatchingGameplayTag(BeyondTags::State_Dead) || ASC->HasMatchingGameplayTag(BeyondTags::State_Uninterruptible)
		|| !Enemy->GetHitReactMontage(Response))
	{
		return false;
	}

	// Light hits: only a flinch when idle / moving, and not every hit of a combo
	if (Response == BeyondTags::Event_Hit_Light)
	{
		const UBeyondEnemyDefinition* Definition = Enemy->GetDefinition();
		const float Cooldown = Definition ? Definition->LightHitReactCooldown : 1.0f;
		const float Now = Enemy->GetWorld()->GetTimeSeconds();
		if (Now - LastLightReactTime < Cooldown || ASC->HasMatchingGameplayTag(BeyondTags::State_Stunned))
		{
			return false;
		}
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
			if (Spec.IsActive() && Ability && Ability->GetAssetTags().HasTagExact(BeyondTags::Ability_Enemy_Attack))
			{
				return false;
			}
		}
	}
	return true;
}

void UBeyondGA_HitReact::CancelOtherAbilities() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ToCancel;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UBeyondGameplayAbility* Ability = Cast<UBeyondGameplayAbility>(Spec.Ability);
		if (Spec.IsActive() && Spec.Handle != CurrentSpecHandle && !(Ability && Ability->bActivateOnGranted))
		{
			ToCancel.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : ToCancel)
	{
		ASC->CancelAbilityHandle(Handle);
	}
}

void UBeyondGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ABeyondEnemyCharacter* Enemy = Cast<ABeyondEnemyCharacter>(GetAvatarActorFromActorInfo());
	const FGameplayTag Response = TriggerEventData ? TriggerEventData->EventTag : BeyondTags::Event_Hit_Light;
	UAnimMontage* Reaction = Enemy ? Enemy->GetHitReactMontage(Response) : nullptr;
	if (!Reaction || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bLight = Response == BeyondTags::Event_Hit_Light;
	if (bLight)
	{
		LastLightReactTime = Enemy->GetWorld()->GetTimeSeconds();
	}
	else
	{
		CancelOtherAbilities();
	}

	if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
	{
		AI->StopMovement();
	}

	float Duration = PlayMontageOnAvatar(Reaction);
	if (Response == BeyondTags::Event_Hit_Stun)
	{
		bStunned = true;
		Enemy->GetAbilitySystemComponent()->AddLooseGameplayTag(BeyondTags::State_Stunned);
		Duration = FMath::Max(Duration, MinStunDuration);
	}

	GetWorld()->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}), FMath::Max(Duration * 0.85f, 0.1f), false);
}

void UBeyondGA_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndTimer);
	}
	if (bStunned)
	{
		bStunned = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(BeyondTags::State_Stunned);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
