// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Projectile.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Weapons/BeyondWeapon.h"

namespace
{
	const FGameplayTag& ShootProjectileTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.ShootProjectile"));
		return Tag;
	}
	const FGameplayTag& MontageTriggerTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Trigger"));
		return Tag;
	}

	// Write a Blueprint variable on the projectile by name, if it has one of that type
	template <typename TPropertyType, typename TValue>
	void SetProjectileVariable(AActor* Projectile, FName Name, const TValue& Value)
	{
		if (TPropertyType* Prop = FindFProperty<TPropertyType>(Projectile->GetClass(), Name))
		{
			Prop->SetValue_InContainer(Projectile, Value);
		}
	}

	void SetProjectileStruct(AActor* Projectile, FName Name, const UScriptStruct* Struct, const void* Value)
	{
		if (FStructProperty* Prop = FindFProperty<FStructProperty>(Projectile->GetClass(), Name); Prop && Prop->Struct == Struct)
		{
			Prop->Struct->CopyScriptStruct(Prop->ContainerPtrToValuePtr<void>(Projectile), Value);
		}
	}
}

UBeyondGA_Projectile::UBeyondGA_Projectile()
{
	InputTag = BeyondTags::Ability_Input_Primary;
	AIMinRange = 300.0f;
	AIMaxRange = 1800.0f;
	AIWeight = 2.0f;
}

void UBeyondGA_Projectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bFired = false;
	bMontageDone = false;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	ASC->GenericGameplayEventCallbacks.FindOrAdd(ShootProjectileTag()).AddUObject(this, &ThisClass::HandleFireEvent);
	ASC->GenericGameplayEventCallbacks.FindOrAdd(MontageTriggerTag()).AddUObject(this, &ThisClass::HandleFireEvent);

	const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const float Duration = (CastMontage && AnimInstance) ? AnimInstance->Montage_Play(CastMontage) : 0.0f;

	if (Duration > 0.0f)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, ActivationSerial);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, CastMontage);
		GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::Fire, FMath::Min(FallbackFireDelay, Duration * 0.9f), false);
	}
	else
	{
		bMontageDone = true;
		Fire();
	}
}

void UBeyondGA_Projectile::HandleFireEvent(const FGameplayEventData* Payload)
{
	Fire();
}

FVector UBeyondGA_Projectile::GetProjectileSpawnLocation_Implementation() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (const USkeletalMeshComponent* Mesh = GetAnimatedMesh(); Mesh && Mesh->DoesSocketExist(SpawnSocket))
	{
		return Mesh->GetSocketLocation(SpawnSocket);
	}

	// A staff with a SpawnPoint component (BP_Weapon_Staff)
	if (const ABeyondWeapon* Weapon = ABeyondWeapon::FindEquippedWeapon(Avatar))
	{
		TInlineComponentArray<USceneComponent*> Components(Weapon);
		for (const USceneComponent* Component : Components)
		{
			if (Component->GetFName() == TEXT("SpawnPoint"))
			{
				return Component->GetComponentLocation();
			}
		}
	}

	return Avatar ? Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 80.0f + FVector(0.0f, 0.0f, 40.0f) : FVector::ZeroVector;
}

void UBeyondGA_Projectile::Fire()
{
	if (bFired || !IsActive())
	{
		return;
	}
	bFired = true;
	GetWorld()->GetTimerManager().ClearTimer(FireTimer);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const FVector SpawnLocation = GetProjectileSpawnLocation();
	const FRotator Aim = GetAimRotation(SpawnLocation);

	FVector TargetLocation = SpawnLocation + Aim.Vector() * MaxRange;
	if (const AActor* FocusTarget = GetAIFocusTarget())
	{
		TargetLocation = FocusTarget->GetActorLocation();
	}

	// Damage travels with the projectile as a spec from our shared damage effect
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UBeyondGE_Damage::StaticClass(), GetAbilityLevel());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Damage, Damage);
		Spec.Data->AddDynamicAssetTag(BeyondTags::DamageType_Projectile);
		Spec.Data->AddDynamicAssetTag(HitResponse.IsValid() ? HitResponse : BeyondTags::Event_Hit_Light);
	}

	const FTransform SpawnTransform(Aim, SpawnLocation);
	AActor* Projectile = GetWorld()->SpawnActorDeferred<AActor>(ProjectileClass, SpawnTransform, Avatar, Cast<APawn>(Avatar),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		SetProjectileVariable<FDoubleProperty>(Projectile, TEXT("Speed"), static_cast<double>(ProjectileSpeed));
		SetProjectileVariable<FFloatProperty>(Projectile, TEXT("Speed"), ProjectileSpeed);
		SetProjectileStruct(Projectile, TEXT("TargetLocation"), TBaseStructure<FVector>::Get(), &TargetLocation);
		SetProjectileStruct(Projectile, TEXT("EfectSpecHandle"), FGameplayEffectSpecHandle::StaticStruct(), &Spec);
		SetProjectileStruct(Projectile, TEXT("EffectSpecHandle"), FGameplayEffectSpecHandle::StaticStruct(), &Spec);
		Projectile->FinishSpawning(SpawnTransform);
	}

	if (bMontageDone)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBeyondGA_Projectile::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial)
{
	if (!IsCurrentActivation(Serial))
	{
		return;
	}
	bMontageDone = true;
	if (!bFired && !bInterrupted)
	{
		Fire();
		return;
	}
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
	}
}

void UBeyondGA_Projectile::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		for (const FGameplayTag& Tag : { ShootProjectileTag(), MontageTriggerTag() })
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Tag))
			{
				Delegate->RemoveAll(this);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
