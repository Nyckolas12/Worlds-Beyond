// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_MeleeCombo.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Weapons/BeyondWeapon.h"

namespace
{
	const FGameplayTag& HitScanStartTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.HitScan.Start"));
		return Tag;
	}
	const FGameplayTag& HitScanEndTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.HitScan.End"));
		return Tag;
	}
	const FGameplayTag& ComboStartTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.ContinueCombo.Start"));
		return Tag;
	}
	const FGameplayTag& ComboEndTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.ContinueCombo.End"));
		return Tag;
	}
}

UBeyondGA_MeleeCombo::UBeyondGA_MeleeCombo()
{
	InputTag = BeyondTags::Ability_Input_Primary;
	AIMinRange = 0.0f;
	AIMaxRange = 220.0f;
	AIWeight = 2.0f;
}

void UBeyondGA_MeleeCombo::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (ComboSteps.IsEmpty() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Listen for the anim notifies' gameplay events on the owner's ASC
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	for (const FGameplayTag& Tag : { HitScanStartTag(), HitScanEndTag(), ComboStartTag(), ComboEndTag() })
	{
		ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddUObject(this, &ThisClass::HandleGameplayEvent);
	}

	PlayStep(0);
}

void UBeyondGA_MeleeCombo::PlayStep(int32 StepIndex)
{
	CurrentStep = StepIndex;
	bComboWindowOpen = false;
	bNextStepQueued = false;
	bGotHitNotify = false;
	bGotComboNotify = false;
	StopHitWindow();

	const FBeyondComboStep& Step = ComboSteps[StepIndex];
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* AnimInstance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Step.Montage || !AnimInstance)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const float Duration = AnimInstance->Montage_Play(Step.Montage, Step.PlayRate);
	if (Duration <= 0.0f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Chain on blend-out so combos flow; the step index lets stale callbacks from earlier steps be ignored
	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &ThisClass::HandleMontageBlendingOut, StepIndex);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Step.Montage);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, StepIndex);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Step.Montage);

	// Fallback timings; cancelled as soon as real notifies arrive
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(HitStartTimer, [this]() { if (!bGotHitNotify) { StartHitWindow(); } }, FMath::Max(Duration * FallbackHitStart, 0.01f), false);
	Timers.SetTimer(HitEndTimer, [this]() { if (!bGotHitNotify) { StopHitWindow(); } }, FMath::Max(Duration * FallbackHitEnd, 0.02f), false);
	Timers.SetTimer(ComboWindowTimer, [this]() { if (!bGotComboNotify) { OpenComboWindow(); } }, FMath::Max(Duration * FallbackComboWindowStart, 0.01f), false);
}

void UBeyondGA_MeleeCombo::HandleGameplayEvent(const FGameplayEventData* Payload)
{
	if (!Payload)
	{
		return;
	}

	const FGameplayTag& Tag = Payload->EventTag;
	if (Tag == HitScanStartTag())
	{
		bGotHitNotify = true;
		StartHitWindow();
	}
	else if (Tag == HitScanEndTag())
	{
		bGotHitNotify = true;
		StopHitWindow();
	}
	else if (Tag == ComboStartTag())
	{
		bGotComboNotify = true;
		OpenComboWindow();
	}
	else if (Tag == ComboEndTag())
	{
		bGotComboNotify = true;
		bComboWindowOpen = false;
	}
}

void UBeyondGA_MeleeCombo::OpenComboWindow()
{
	bComboWindowOpen = true;

	// The AI "presses" by queueing immediately so its combos flow
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (Pawn && !Pawn->IsPlayerControlled())
	{
		bNextStepQueued = true;
	}
}

void UBeyondGA_MeleeCombo::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if (bComboWindowOpen)
	{
		bNextStepQueued = true;
	}
}

void UBeyondGA_MeleeCombo::StartHitWindow()
{
	if (bHitWindowOpen)
	{
		return;
	}
	bHitWindowOpen = true;

	const FBeyondComboStep& Step = ComboSteps[CurrentStep];
	const FGameplayTag Response = Step.HitResponse.IsValid() ? Step.HitResponse : BeyondTags::Event_Hit_Light;

	if (ABeyondWeapon* Weapon = ABeyondWeapon::FindEquippedWeapon(GetAvatarActorFromActorInfo()))
	{
		Weapon->BeginMeleeScan(Step.Damage, Response);
	}
	else
	{
		UnarmedHits.Reset();
		GetWorld()->GetTimerManager().SetTimer(UnarmedSweepTimer, this, &ThisClass::UnarmedSweep, 0.03f, true, 0.0f);
	}
}

void UBeyondGA_MeleeCombo::StopHitWindow()
{
	bHitWindowOpen = false;

	if (ABeyondWeapon* Weapon = ABeyondWeapon::FindEquippedWeapon(GetAvatarActorFromActorInfo()))
	{
		Weapon->HitScanEnd();
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnarmedSweepTimer);
	}
}

void UBeyondGA_MeleeCombo::UnarmedSweep()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !ComboSteps.IsValidIndex(CurrentStep))
	{
		return;
	}

	const FVector Start = Avatar->GetActorLocation();
	const FVector End = Start + Avatar->GetActorForwardVector() * UnarmedReach;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondUnarmedSweep), false, Avatar);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(UnarmedRadius), Params);

	const FBeyondComboStep& Step = ComboSteps[CurrentStep];
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && !UnarmedHits.Contains(HitActor) && UBeyondCombatLibrary::AreHostile(Avatar, HitActor))
		{
			UnarmedHits.Add(HitActor);
			ApplyDamageToTarget(HitActor, Step.Damage, BeyondTags::DamageType_Melee, Step.HitResponse.IsValid() ? Step.HitResponse : BeyondTags::Event_Hit_Light);
		}
	}
}

void UBeyondGA_MeleeCombo::HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 StepIndex)
{
	if (!IsActive() || StepIndex != CurrentStep || bInterrupted)
	{
		return;
	}

	StopHitWindow();
	if (bNextStepQueued && ComboSteps.IsValidIndex(CurrentStep + 1))
	{
		PlayStep(CurrentStep + 1);
	}
}

void UBeyondGA_MeleeCombo::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 StepIndex)
{
	// A later step already started (chained on blend-out)
	if (!IsActive() || StepIndex != CurrentStep)
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
}

void UBeyondGA_MeleeCombo::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopHitWindow();

	if (const UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(HitStartTimer);
		Timers.ClearTimer(HitEndTimer);
		Timers.ClearTimer(ComboWindowTimer);
	}

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		for (const FGameplayTag& Tag : { HitScanStartTag(), HitScanEndTag(), ComboStartTag(), ComboEndTag() })
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Tag))
			{
				Delegate->RemoveAll(this);
			}
		}

		// Stop the montage if we were cancelled mid-swing
		if (bWasCancelled && ComboSteps.IsValidIndex(CurrentStep))
		{
			if (const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
			{
				if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
				{
					AnimInstance->Montage_Stop(0.2f, ComboSteps[CurrentStep].Montage);
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
