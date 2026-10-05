// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_EquipWeapon.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UBeyondGA_EquipWeapon::UBeyondGA_EquipWeapon()
{
	bAIUsable = false;

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = BeyondTags::Event_Weapon_Equipped;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

const FBeyondWeaponLoadout* UBeyondGA_EquipWeapon::FindLoadout(const FGameplayTagContainer& Tags) const
{
	for (const FBeyondWeaponLoadout& Loadout : Loadouts)
	{
		if (Loadout.WeaponTag.IsValid() && Tags.HasTag(Loadout.WeaponTag))
		{
			return &Loadout;
		}
	}
	return nullptr;
}

void UBeyondGA_EquipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bInstant = TriggerEventData && TriggerEventData->EventMagnitude > 0.0f;
	const FBeyondWeaponLoadout* Loadout = TriggerEventData ? FindLoadout(TriggerEventData->TargetTags) : nullptr;

	if (EquippedWeapon.IsValid() && (!Loadout || Loadout->WeaponTag == EquippedTag))
	{
		// Same weapon again (or an unknown one): sheathe
		BeginUnequip(bInstant);
		return;
	}

	if (!Loadout)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Swapping weapons: put the current one away at once, then draw the new one
	if (EquippedWeapon.IsValid())
	{
		PutAwayWeapon();
	}
	BeginEquip(*Loadout, bInstant);
}

void UBeyondGA_EquipWeapon::BeginEquip(const FBeyondWeaponLoadout& Loadout, bool bInstant)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	if (!Character || !Mesh || !Loadout.WeaponClass)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Weapon = GetWorld()->SpawnActor<AActor>(Loadout.WeaponClass, Mesh->GetComponentTransform(), Params);
	if (!Weapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Attached (so hit detection finds it) but hidden until the hand reaches for it
	Weapon->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Mesh->DoesSocketExist(Loadout.AttachSocket) ? Loadout.AttachSocket : NAME_None);
	Weapon->SetActorEnableCollision(false);
	EquippedWeapon = Weapon;
	EquippedTag = Loadout.WeaponTag;
	PendingAnimClass = Loadout.ArmedAnimClass;
	PendingWalkSpeed = Loadout.ArmedWalkSpeed;

	const float Duration = bInstant ? 0.0f : PlayMontageOnAvatar(Loadout.EquipMontage);
	if (Duration <= 0.0f)
	{
		ShowWeapon();
		Finish();
		return;
	}

	Weapon->SetActorHiddenInGame(true);
	GetWorld()->GetTimerManager().SetTimer(AttachTimer, this, &ThisClass::ShowWeapon, FMath::Clamp(Loadout.AttachTime, 0.01f, Duration), false);
	FinishAfterMontage(Loadout.EquipMontage, Duration);
}

void UBeyondGA_EquipWeapon::BeginUnequip(bool bInstant)
{
	const FBeyondWeaponLoadout* Loadout = nullptr;
	for (const FBeyondWeaponLoadout& Candidate : Loadouts)
	{
		if (Candidate.WeaponTag == EquippedTag)
		{
			Loadout = &Candidate;
			break;
		}
	}

	PendingAnimClass = UnarmedAnimClass;
	PendingWalkSpeed = UnarmedWalkSpeed;

	const float Duration = (bInstant || !Loadout) ? 0.0f : PlayMontageOnAvatar(Loadout->UnequipMontage);
	if (Duration <= 0.0f)
	{
		PutAwayWeapon();
		Finish();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(AttachTimer, this, &ThisClass::PutAwayWeapon, FMath::Clamp(Loadout->DetachTime, 0.01f, Duration), false);
	FinishAfterMontage(Loadout->UnequipMontage, Duration);
}

void UBeyondGA_EquipWeapon::ShowWeapon()
{
	if (AActor* Weapon = EquippedWeapon.Get())
	{
		Weapon->SetActorHiddenInGame(false);
	}
}

void UBeyondGA_EquipWeapon::PutAwayWeapon()
{
	if (AActor* Weapon = EquippedWeapon.Get())
	{
		Weapon->Destroy();
	}
	EquippedWeapon.Reset();
	EquippedTag = FGameplayTag();
}

void UBeyondGA_EquipWeapon::FinishAfterMontage(UAnimMontage* Montage, float Duration)
{
	if (UAnimInstance* AnimInstance = GetAnimatedMesh()->GetAnimInstance())
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, ActivationSerial);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	}
	// In case the montage is replaced without ending cleanly
	GetWorld()->GetTimerManager().SetTimer(FinishTimer, this, &ThisClass::Finish, Duration + 0.5f, false);
}

void UBeyondGA_EquipWeapon::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial)
{
	if (IsCurrentActivation(Serial))
	{
		Finish();
	}
}

void UBeyondGA_EquipWeapon::Finish()
{
	if (!IsActive())
	{
		return;
	}

	// Interrupted before the hand got there: still complete the swap so the state stays consistent
	if (GetWorld()->GetTimerManager().IsTimerActive(AttachTimer))
	{
		GetWorld()->GetTimerManager().ClearTimer(AttachTimer);
		if (EquippedWeapon.IsValid() && EquippedWeapon->IsHidden())
		{
			ShowWeapon();
		}
		else if (PendingAnimClass == UnarmedAnimClass)
		{
			PutAwayWeapon();
		}
	}

	ApplyStance(PendingAnimClass, PendingWalkSpeed);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBeyondGA_EquipWeapon::ApplyStance(TSubclassOf<UAnimInstance> AnimClass, float WalkSpeed)
{
	if (USkeletalMeshComponent* Mesh = GetAnimatedMesh(); Mesh && AnimClass && Mesh->GetAnimClass() != AnimClass)
	{
		Mesh->SetAnimInstanceClass(AnimClass);
	}

	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()); Character && WalkSpeed > 0.0f)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
}

void UBeyondGA_EquipWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttachTimer);
		World->GetTimerManager().ClearTimer(FinishTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
