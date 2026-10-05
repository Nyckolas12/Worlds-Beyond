// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_EquipWeapon.h"
#include "WorldBeyond.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Weapons/BeyondWeapon.h"

namespace
{
	const FName DefaultSlotName(TEXT("DefaultSlot"));
	constexpr float AutoTickInterval = 0.5f;
}

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

const FBeyondWeaponLoadout* UBeyondGA_EquipWeapon::GetEquippedLoadout() const
{
	return EquippedTag.IsValid() ? FindLoadout(FGameplayTagContainer(EquippedTag)) : nullptr;
}

void UBeyondGA_EquipWeapon::RequestWeaponAction(FGameplayTag Action, bool bInstant)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !EquippedTag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = BeyondTags::Event_Weapon_Equipped;
	Payload.Instigator = Avatar;
	Payload.Target = Avatar;
	Payload.TargetTags.AddTag(EquippedTag);
	if (Action.IsValid())
	{
		Payload.InstigatorTags.AddTag(Action);
	}
	if (bInstant)
	{
		Payload.InstigatorTags.AddTag(BeyondTags::Weapon_Action_Instant);
	}
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, Payload.EventTag, Payload);
}

void UBeyondGA_EquipWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTagContainer Actions = TriggerEventData ? TriggerEventData->InstigatorTags : FGameplayTagContainer();
	const bool bInstant = TriggerEventData && (TriggerEventData->EventMagnitude > 0.0f || Actions.HasTagExact(BeyondTags::Weapon_Action_Instant));
	const bool bWantDraw = Actions.HasTagExact(BeyondTags::Weapon_Action_Draw);
	const bool bWantSheathe = Actions.HasTagExact(BeyondTags::Weapon_Action_Sheathe);
	const FBeyondWeaponLoadout* Requested = TriggerEventData ? FindLoadout(TriggerEventData->TargetTags) : nullptr;

	// Asked for a different weapon than the one carried: the old one goes away
	if (Requested && EquippedWeapon.IsValid() && Requested->WeaponTag != EquippedTag)
	{
		DestroyWeapon();
	}

	if (!EquippedWeapon.IsValid())
	{
		if (!Requested || !SpawnWeapon(*Requested))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		// Spawning with the default weapon: it starts where the ability says, no animation
		if (bInstant && !bWantDraw && !bWantSheathe)
		{
			if (!bSpawnHolstered)
			{
				PlaceInHand();
			}
			ApplyStance(bDrawn);
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
	}

	const bool bDraw = bWantDraw || (!bWantSheathe && !bDrawn);
	if (bDraw == bDrawn)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (bDraw)
	{
		BeginDraw(bInstant);
	}
	else
	{
		BeginSheathe(bInstant);
	}
}

bool UBeyondGA_EquipWeapon::SpawnWeapon(const FBeyondWeaponLoadout& Loadout)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	if (!Character || !Mesh || !Loadout.WeaponClass)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Weapon = GetWorld()->SpawnActor<AActor>(Loadout.WeaponClass, Mesh->GetComponentTransform(), Params);
	if (!Weapon)
	{
		return false;
	}

	Weapon->SetActorEnableCollision(false);
	EquippedWeapon = Weapon;
	EquippedTag = Loadout.WeaponTag;
	PlaceInHolster();
	return true;
}

void UBeyondGA_EquipWeapon::DestroyWeapon()
{
	if (AActor* Weapon = EquippedWeapon.Get())
	{
		Weapon->Destroy();
	}
	EquippedWeapon.Reset();
	EquippedTag = FGameplayTag();
	bDrawn = false;
}

void UBeyondGA_EquipWeapon::PlaceInHand()
{
	AActor* Weapon = EquippedWeapon.Get();
	USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	const FBeyondWeaponLoadout* Loadout = GetEquippedLoadout();
	if (!Weapon || !Mesh || !Loadout)
	{
		return;
	}

	Weapon->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		Mesh->DoesSocketExist(Loadout->AttachSocket) ? Loadout->AttachSocket : NAME_None);
	if (ABeyondWeapon* BeyondWeapon = Cast<ABeyondWeapon>(Weapon))
	{
		BeyondWeapon->bHolstered = false;
	}
	bDrawn = true;
}

void UBeyondGA_EquipWeapon::PlaceInHolster()
{
	AActor* Weapon = EquippedWeapon.Get();
	USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	const FBeyondWeaponLoadout* Loadout = GetEquippedLoadout();
	if (!Weapon || !Mesh || !Loadout)
	{
		return;
	}

	if (!Loadout->HolsterSocket.IsNone() && Mesh->DoesSocketExist(Loadout->HolsterSocket))
	{
		// A socket made for it (tuned in the Skeleton editor)
		Weapon->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Loadout->HolsterSocket);
	}
	else
	{
		Weapon->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Mesh->DoesSocketExist(Loadout->HolsterBone) ? Loadout->HolsterBone : NAME_None);
		Weapon->SetActorRelativeLocation(Loadout->HolsterOffset.GetLocation());
		Weapon->SetActorRelativeRotation(Loadout->HolsterOffset.GetRotation());
	}

	if (ABeyondWeapon* BeyondWeapon = Cast<ABeyondWeapon>(Weapon))
	{
		BeyondWeapon->HitScanEnd();
		BeyondWeapon->bHolstered = true;
	}
	Weapon->SetActorHiddenInGame(false);
	bDrawn = false;
}

void UBeyondGA_EquipWeapon::ApplyStance(bool bArmed)
{
	const FBeyondWeaponLoadout* Loadout = GetEquippedLoadout();
	UAnimSequenceBase* Idle = bArmed ? (Loadout ? Loadout->ArmedIdle.Get() : nullptr) : UnarmedIdle.Get();
	const float WalkSpeed = bArmed ? (Loadout ? Loadout->ArmedWalkSpeed : 0.0f) : UnarmedWalkSpeed;

	// The anim Blueprint keeps running (no class swap), so montages and bindings survive the change
	const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr; AnimInstance && Idle)
	{
		if (FObjectProperty* Prop = FindFProperty<FObjectProperty>(AnimInstance->GetClass(), IdleVariable); Prop && Idle->IsA(Prop->PropertyClass))
		{
			Prop->SetObjectPropertyValue_InContainer(AnimInstance, Idle);
		}
	}

	if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()); Character && WalkSpeed > 0.0f)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
}

void UBeyondGA_EquipWeapon::BeginDraw(bool bInstant)
{
	PendingDirection = 1;
	bPendingMoveDone = false;
	LastCombatTime = GetWorld()->GetTimeSeconds();

	const FBeyondWeaponLoadout* Loadout = GetEquippedLoadout();
	const float Rate = Loadout ? Loadout->AnimationPlayRate : 1.0f;
	const float Duration = (bInstant || !Loadout) ? 0.0f : PlayWeaponAnimation(Loadout->DrawAnimation, Rate);
	if (Duration <= 0.0f)
	{
		Finish();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(MoveTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		PlaceInHand();
		bPendingMoveDone = true;
	}), FMath::Clamp(Loadout->GrabTime / Rate, 0.01f, Duration), false);
	GetWorld()->GetTimerManager().SetTimer(FinishTimer, this, &ThisClass::Finish, Duration + 0.25f, false);
}

void UBeyondGA_EquipWeapon::BeginSheathe(bool bInstant)
{
	PendingDirection = -1;
	bPendingMoveDone = false;

	const FBeyondWeaponLoadout* Loadout = GetEquippedLoadout();
	const float Rate = Loadout ? Loadout->AnimationPlayRate : 1.0f;
	const float Duration = (bInstant || !Loadout) ? 0.0f : PlayWeaponAnimation(Loadout->SheatheAnimation, Rate);
	if (Duration <= 0.0f)
	{
		Finish();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(MoveTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		PlaceInHolster();
		bPendingMoveDone = true;
	}), FMath::Clamp(Loadout->ReleaseTime / Rate, 0.01f, Duration), false);
	GetWorld()->GetTimerManager().SetTimer(FinishTimer, this, &ThisClass::Finish, Duration + 0.25f, false);
}

FName UBeyondGA_EquipWeapon::ResolveSlot(UAnimInstance* AnimInstance) const
{
	if (AnimationSlot.IsNone() || AnimationSlot == DefaultSlotName)
	{
		return DefaultSlotName;
	}

	// Only use the slot if the anim Blueprint actually has a Slot node for it; otherwise nothing would play
	if (const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(AnimInstance->GetClass()))
	{
		for (const FStructProperty* Prop : AnimClass->GetAnimNodeProperties())
		{
			if (Prop && Prop->Struct && Prop->Struct->IsChildOf(FAnimNode_Slot::StaticStruct())
				&& Prop->ContainerPtrToValuePtr<FAnimNode_Slot>(AnimInstance)->SlotName == AnimationSlot)
			{
				return AnimationSlot;
			}
		}
	}

	static bool bWarned = false;
	if (!bWarned)
	{
		bWarned = true;
		UE_LOG(LogBeyond, Warning, TEXT("%s has no '%s' slot; draw / sheathe play full body until it's added (see Docs/GAS_Prototype.md)"),
			*GetNameSafe(AnimInstance->GetClass()), *AnimationSlot.ToString());
	}
	return DefaultSlotName;
}

float UBeyondGA_EquipWeapon::PlayWeaponAnimation(UAnimSequenceBase* Animation, float PlayRate)
{
	const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Animation || !AnimInstance || PlayRate <= 0.0f)
	{
		return 0.0f;
	}

	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (Montage)
	{
		if (AnimInstance->Montage_Play(Montage, PlayRate) <= 0.0f)
		{
			return 0.0f;
		}
	}
	else
	{
		Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Animation, ResolveSlot(AnimInstance), 0.2f, 0.25f, PlayRate);
	}
	if (!Montage)
	{
		return 0.0f;
	}

	PlayingMontage = Montage;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, ActivationSerial);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return Montage->GetPlayLength() / PlayRate;
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

	// Cut short (an attack took over the upper body): still finish the move so the weapon ends up somewhere sensible
	if (!bPendingMoveDone)
	{
		if (PendingDirection > 0)
		{
			PlaceInHand();
		}
		else if (PendingDirection < 0)
		{
			PlaceInHolster();
		}
		bPendingMoveDone = true;
	}

	ApplyStance(bDrawn);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBeyondGA_EquipWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveTimer);
		World->GetTimerManager().ClearTimer(FinishTimer);
	}

	if (bWasCancelled && PlayingMontage.IsValid())
	{
		const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
		if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Stop(0.2f, PlayingMontage.Get());
		}
	}
	PlayingMontage.Reset();
	PendingDirection = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBeyondGA_EquipWeapon::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	if (!IsInstantiated() || !GetWorld())
	{
		return;
	}
	BindAnimInstance();

	// Bound to the character, not to this ability: GAS clears every timer of an ability object when it ends
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (Avatar)
	{
		TWeakObjectPtr<UBeyondGA_EquipWeapon> WeakThis(this);
		GetWorld()->GetTimerManager().SetTimer(AutoTimer, FTimerDelegate::CreateWeakLambda(Avatar, [WeakThis]()
		{
			if (UBeyondGA_EquipWeapon* Ability = WeakThis.Get())
			{
				Ability->AutoTick();
			}
		}), AutoTickInterval, true, AutoTickInterval);
	}
}

UAnimInstance* UBeyondGA_EquipWeapon::BindAnimInstance()
{
	// Attack montages that need the weapon draw it on the spot (the Blueprint sword combo, the buddy's combo)
	const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (AnimInstance != BoundAnimInstance.Get())
	{
		if (UAnimInstance* Previous = BoundAnimInstance.Get())
		{
			Previous->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleMontageStarted);
		}
		if (AnimInstance)
		{
			AnimInstance->OnMontageStarted.AddUniqueDynamic(this, &ThisClass::HandleMontageStarted);
		}
		BoundAnimInstance = AnimInstance;
	}
	return AnimInstance;
}

void UBeyondGA_EquipWeapon::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (IsInstantiated())
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AutoTimer);
		}
		if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
		{
			AnimInstance->OnMontageStarted.RemoveDynamic(this, &ThisClass::HandleMontageStarted);
		}
		DestroyWeapon();
	}
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UBeyondGA_EquipWeapon::AutoTick()
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Character || !ASC)
	{
		return;
	}

	const UAnimInstance* AnimInstance = BindAnimInstance();

	if (!EquippedWeapon.IsValid() || IsActive() || UBeyondCombatLibrary::IsActorDead(Character) || ASC->HasMatchingGameplayTag(BeyondTags::State_Duo))
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (ASC->HasMatchingGameplayTag(BeyondTags::Ability_Active))
	{
		LastCombatTime = Now;
	}

	if (!bDrawn)
	{
		if (AutoDrawRadius > 0.0f && !FindHostilesInRadius(Character->GetActorLocation(), AutoDrawRadius).IsEmpty())
		{
			UE_LOG(LogBeyond, Verbose, TEXT("%s: enemies close, drawing"), *Character->GetName());
			RequestWeaponAction(BeyondTags::Weapon_Action_Draw, false);
		}
		return;
	}

	if (AutoSheatheDelay <= 0.0f)
	{
		return;
	}
	if ((AutoSheatheRadius > 0.0f && !FindHostilesInRadius(Character->GetActorLocation(), AutoSheatheRadius).IsEmpty())
		|| (AnimInstance && AnimInstance->IsAnyMontagePlaying()))
	{
		LastCombatTime = Now;
		return;
	}
	if (Now - LastCombatTime >= AutoSheatheDelay)
	{
		UE_LOG(LogBeyond, Verbose, TEXT("%s: quiet for %.1f s, sheathing"), *Character->GetName(), Now - LastCombatTime);
		RequestWeaponAction(BeyondTags::Weapon_Action_Sheathe, false);
	}
}

void UBeyondGA_EquipWeapon::HandleMontageStarted(UAnimMontage* Montage)
{
	if (!Montage || !DrawOnMontages.Contains(Montage))
	{
		return;
	}
	LastCombatTime = GetWorld()->GetTimeSeconds();
	if (EquippedWeapon.IsValid() && !bDrawn)
	{
		QuickDraw();
	}
}

void UBeyondGA_EquipWeapon::QuickDraw()
{
	// A slow draw / sheathe in progress gives way
	if (IsActive())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
	PlaceInHand();
	ApplyStance(true);
	UE_LOG(LogBeyond, Verbose, TEXT("%s: quick-draw for an attack"), *GetNameSafe(GetAvatarActorFromActorInfo()));
}
