// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_MeleeCombo.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BeyondGameplayTags.h"
#include "Characters/BeyondCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Game/BeyondCombatSubsystem.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
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

	const FName ComboFallbackSlot(TEXT("DefaultSlot"));
	constexpr float LegsUpdateInterval = 0.05f;
	// Montage Notify Window (UAnimNotify_PlayMontageNotifyWindow, the Play Montage node's On Notify Begin / End),
	// matched by class name to avoid depending on its AnimGraphRuntime header
	const TCHAR* MontageNotifyWindowClass = TEXT("PlayMontageNotifyWindow");
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

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		AnimInstance->OnMontageStarted.AddUniqueDynamic(this, &ThisClass::HandleAnyMontageStarted);
		AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::HandleMontageNotifyBegin);
		AnimInstance->OnPlayMontageNotifyEnd.AddUniqueDynamic(this, &ThisClass::HandleMontageNotifyEnd);
		BoundAnimInstance = AnimInstance;
	}

	PlayStep(0);
	if (IsActive())
	{
		PlayVoiceLine();
	}
}

void UBeyondGA_MeleeCombo::PlayVoiceLine()
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!ComboVoiceLine || !Pawn)
	{
		return;
	}

	// The player hears their own demigod up close; the buddy's line comes from where it stands
	if (Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(this, ComboVoiceLine);
	}
	else if (USkeletalMeshComponent* Mesh = GetAnimatedMesh())
	{
		UGameplayStatics::SpawnSoundAttached(ComboVoiceLine, Mesh);
	}
	++VoiceLinesPlayed;
}

bool UBeyondGA_MeleeCombo::IsMoving() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar && Avatar->GetVelocity().Size2D() > MovingSpeed;
}

bool UBeyondGA_MeleeCombo::IsComboMontage(UAnimMontage* Montage) const
{
	const UAnimMontage* Source = UBeyondCombatSubsystem::GetMontageSource(Montage);
	return Source && ComboSteps.ContainsByPredicate([Source](const FBeyondComboStep& Step) { return Step.Montage == Source; });
}

void UBeyondGA_MeleeCombo::HandleAnyMontageStarted(UAnimMontage* Montage)
{
	if (IsActive() && Montage && !IsComboMontage(Montage))
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

bool UBeyondGA_MeleeCombo::IsComboWindowNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload) const
{
	// Only the swing's own windows: the muted footwork copy plays the same ones
	return IsActive() && bWindowsFromMontage && !bComboStopped && NotifyName == ComboWindowNotifyName
		&& BranchingPointPayload.SequenceAsset && BranchingPointPayload.SequenceAsset == SwingMontage.Get();
}

void UBeyondGA_MeleeCombo::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (IsComboWindowNotify(NotifyName, BranchingPointPayload))
	{
		bGotComboNotify = true;
		OpenComboWindow();
	}
}

void UBeyondGA_MeleeCombo::HandleMontageNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (IsComboWindowNotify(NotifyName, BranchingPointPayload))
	{
		bGotComboNotify = true;
		EndComboWindow();
	}
}

void UBeyondGA_MeleeCombo::PlayStep(int32 StepIndex)
{
	CurrentStep = StepIndex;
	bComboWindowOpen = false;
	bNextStepQueued = false;
	bGotHitNotify = false;
	bGotComboNotify = false;
	bComboStopped = false;
	StopHitWindow();

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.ClearTimer(ComboCloseTimer);
	Timers.ClearTimer(LegsTimer);

	const FBeyondComboStep& Step = ComboSteps[StepIndex];
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!Step.Montage || !AnimInstance)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// The swing on the upper body, plus a muted full-body copy for the footwork while standing (see UpdateLegs)
	UAnimMontage* Swing = Step.Montage;
	UAnimMontage* Legs = nullptr;
	const FName AuthoredSlot = Step.Montage->SlotAnimTracks.IsEmpty() ? ComboFallbackSlot : Step.Montage->SlotAnimTracks[0].SlotName;
	UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(this);
	if (bLegsFollowMovement && Combat && AuthoredSlot != UpperBodySlot && UBeyondCombatLibrary::HasAnimSlot(AnimInstance, UpperBodySlot))
	{
		Swing = Combat->GetMontageVariant(Step.Montage, UpperBodySlot, false);
		Legs = Combat->GetMontageVariant(Step.Montage, AuthoredSlot, true);
	}
	SwingMontage = Swing;
	LegsMontage = Legs;

	// The montage's own Montage Notify Windows (ResumeComboWindow) decide the combo window when it has them
	bWindowsFromMontage = !ComboWindowNotifyName.IsNone() && Step.Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Notify)
	{
		return Notify.NotifyStateClass && Notify.NotifyStateClass->GetClass()->GetName().Contains(MontageNotifyWindowClass);
	});

	const float Duration = AnimInstance->Montage_Play(Swing, Step.PlayRate);
	if (Duration <= 0.0f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (Legs)
	{
		// Not stopping the swing: both copies are in the same slot group
		if (!IsMoving())
		{
			AnimInstance->Montage_Play(Legs, Step.PlayRate, EMontagePlayReturnType::MontageLength, 0.0f, false);
		}
		Timers.SetTimer(LegsTimer, this, &ThisClass::UpdateLegs, LegsUpdateInterval, true);
	}

	// Chain on blend-out so combos flow; the step index lets stale callbacks from earlier steps be ignored
	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &ThisClass::HandleMontageBlendingOut, StepIndex, ActivationSerial);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Swing);

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, StepIndex, ActivationSerial);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Swing);

	// Fallback timings; cancelled as soon as real notifies arrive
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

	// A missed window already ended the swing; it's only blending out
	if (bComboStopped)
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
	else if (bWindowsFromMontage)
	{
		// The montage's Montage Notify Windows are the combo window; these events would only disagree with them
		return;
	}
	else if (Tag == ComboStartTag())
	{
		bGotComboNotify = true;
		OpenComboWindow();
	}
	else if (Tag == ComboEndTag())
	{
		bGotComboNotify = true;
		EndComboWindow();
	}
}

void UBeyondGA_MeleeCombo::EndComboWindow()
{
	// Keep the window open a little longer than authored, in proportion to its length
	const float Extra = bComboWindowOpen ? ComboWindowExtension * (GetWorld()->GetTimeSeconds() - ComboWindowOpenedTime) : 0.0f;
	if (Extra > KINDA_SMALL_NUMBER)
	{
		GetWorld()->GetTimerManager().SetTimer(ComboCloseTimer, this, &ThisClass::CloseComboWindow, Extra, false);
	}
	else
	{
		CloseComboWindow();
	}
}

UAnimInstance* UBeyondGA_MeleeCombo::GetAnimInstance() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	USkeletalMeshComponent* Mesh = nullptr;
	if (const ABeyondCharacterBase* BeyondCharacter = Cast<ABeyondCharacterBase>(Avatar))
	{
		Mesh = BeyondCharacter->GetCombatMesh();
	}
	else if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		Mesh = Character->GetMesh();
	}
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

void UBeyondGA_MeleeCombo::CloseComboWindow()
{
	const bool bWasOpen = bComboWindowOpen;
	bComboWindowOpen = false;

	// One-montage combo: no press during the window ends the combo here
	if (bWasOpen && bStopIfComboWindowMissed && !bNextStepQueued && ComboSteps.IsValidIndex(CurrentStep))
	{
		UE_LOG(LogBeyond, Verbose, TEXT("%s: combo window closed without a press, combo ends"), *GetNameSafe(GetAvatarActorFromActorInfo()));
		StopHitWindow();
		bComboStopped = true;
		StopStepMontages(MissedWindowBlendOutTime);
	}
	else if (bWasOpen)
	{
		UE_LOG(LogBeyond, Verbose, TEXT("%s: combo window closed, next swing %s"), *GetNameSafe(GetAvatarActorFromActorInfo()),
			bNextStepQueued ? TEXT("queued") : TEXT("not queued"));
	}
	else if (bStopIfComboWindowMissed)
	{
		// The press kept this montage going; the next window needs a fresh one
		bNextStepQueued = false;
	}
}

void UBeyondGA_MeleeCombo::OpenComboWindow()
{
	// The previous window is still open: its extra time hasn't run out, or its end marker never came. Settle it
	// first, so a missing end can't chain the next swing by itself
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (Timers.IsTimerActive(ComboCloseTimer) || bComboWindowOpen)
	{
		Timers.ClearTimer(ComboCloseTimer);
		CloseComboWindow();
		if (bComboStopped)
		{
			return;
		}
	}

	bComboWindowOpen = true;
	ComboWindowOpenedTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBeyond, Verbose, TEXT("%s: combo window open"), *GetNameSafe(GetAvatarActorFromActorInfo()));

	// The AI "presses" by queueing immediately so its combos flow
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (bAIChainsCombo && Pawn && !Pawn->IsPlayerControlled())
	{
		bNextStepQueued = true;
	}
}

void UBeyondGA_MeleeCombo::UpdateLegs()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	UAnimMontage* Swing = SwingMontage.Get();
	UAnimMontage* Legs = LegsMontage.Get();
	if (!AnimInstance || !Swing || !Legs || bComboStopped || !AnimInstance->Montage_IsPlaying(Swing) || !ComboSteps.IsValidIndex(CurrentStep))
	{
		return;
	}

	const bool bLegsPlaying = AnimInstance->GetActiveInstanceForMontage(Legs) != nullptr;
	if (IsMoving())
	{
		// Walking / running: the legs go back to the locomotion under the upper-body swing
		if (bLegsPlaying)
		{
			AnimInstance->Montage_Stop(LegsBlendTime, Legs);
		}
	}
	else if (!bLegsPlaying)
	{
		// Standing again: bring the footwork back in, in sync with the swing
		AnimInstance->Montage_PlayWithBlendIn(Legs, FAlphaBlendArgs(LegsBlendTime), ComboSteps[CurrentStep].PlayRate,
			EMontagePlayReturnType::MontageLength, AnimInstance->Montage_GetPosition(Swing), false);
	}
}

void UBeyondGA_MeleeCombo::StopStepMontages(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}
	for (UAnimMontage* Montage : { SwingMontage.Get(), LegsMontage.Get() })
	{
		if (Montage && AnimInstance->GetActiveInstanceForMontage(Montage))
		{
			AnimInstance->Montage_Stop(BlendOutTime, Montage);
		}
	}
}

void UBeyondGA_MeleeCombo::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if (bComboWindowOpen)
	{
		bNextStepQueued = true;
		UE_LOG(LogBeyond, Verbose, TEXT("%s: press inside the combo window, next swing queued"), *GetNameSafe(GetAvatarActorFromActorInfo()));
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

void UBeyondGA_MeleeCombo::HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 StepIndex, int32 Serial)
{
	if (!IsCurrentActivation(Serial) || StepIndex != CurrentStep || bInterrupted)
	{
		return;
	}

	StopHitWindow();
	if (bNextStepQueued && ComboSteps.IsValidIndex(CurrentStep + 1))
	{
		PlayStep(CurrentStep + 1);
	}
}

void UBeyondGA_MeleeCombo::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 StepIndex, int32 Serial)
{
	// A later step already started (chained on blend-out), or this is an earlier activation's montage
	if (!IsCurrentActivation(Serial) || StepIndex != CurrentStep)
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
		Timers.ClearTimer(ComboCloseTimer);
		Timers.ClearTimer(LegsTimer);
	}

	if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		AnimInstance->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleAnyMontageStarted);
		AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::HandleMontageNotifyBegin);
		AnimInstance->OnPlayMontageNotifyEnd.RemoveDynamic(this, &ThisClass::HandleMontageNotifyEnd);
	}
	BoundAnimInstance.Reset();

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		for (const FGameplayTag& Tag : { HitScanStartTag(), HitScanEndTag(), ComboStartTag(), ComboEndTag() })
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Tag))
			{
				Delegate->RemoveAll(this);
			}
		}

		// Stop the swing if we were cancelled mid-swing; the footwork copy never outlives the combo
		if (bWasCancelled)
		{
			StopStepMontages(0.2f);
		}
		else if (UAnimInstance* AnimInstance = GetAnimInstance(); AnimInstance && LegsMontage.IsValid() && AnimInstance->GetActiveInstanceForMontage(LegsMontage.Get()))
		{
			AnimInstance->Montage_Stop(0.2f, LegsMontage.Get());
		}
	}
	SwingMontage.Reset();
	LegsMontage.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
