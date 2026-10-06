// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/BeyondGameplayAbility.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystem/BeyondGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "BeyondGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Characters/BeyondCharacterBase.h"

UBeyondGameplayAbility::UBeyondGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

ABeyondCharacterBase* UBeyondGameplayAbility::GetBeyondCharacter() const
{
	return Cast<ABeyondCharacterBase>(GetAvatarActorFromActorInfo());
}

AActor* UBeyondGameplayAbility::GetAIFocusTarget() const
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	return AIController ? AIController->GetFocusActor() : nullptr;
}

FRotator UBeyondGameplayAbility::GetAimRotation(FVector Origin) const
{
	if (const AActor* Target = GetAIFocusTarget())
	{
		// Aim at the middle of the target's collision
		FVector BoundsOrigin, BoundsExtent;
		Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);
		return (BoundsOrigin - Origin).Rotation();
	}

	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		// Trace from the camera so projectiles land on the crosshair, not parallel to it (same trace as the crosshair)
		FHitResult Hit;
		UBeyondCombatLibrary::TraceAlongView(Pawn, 10000.0f, Hit);
		return ((Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd) - Origin).Rotation();
	}

	return Pawn ? Pawn->GetActorRotation() : FRotator::ZeroRotator;
}

bool UBeyondGameplayAbility::ApplyDamageToTarget(AActor* Target, float Amount, FGameplayTag DamageType, FGameplayTag HitResponse, bool bUnblockable)
{
	return UBeyondCombatLibrary::ApplyDamage(GetAvatarActorFromActorInfo(), Target, Amount * GetLevelDamageScale(), DamageType, HitResponse, bUnblockable,
		GetAvatarActorFromActorInfo());
}

float UBeyondGameplayAbility::GetLevelDamageScale() const
{
	return 1.0f + DamagePerLevel * FMath::Max(GetAbilityLevel() - 1, 0);
}

float UBeyondGameplayAbility::GetLevelCooldownScale(int32 Level) const
{
	return FMath::Max(0.4f, 1.0f - CooldownReductionPerLevel * FMath::Max(Level - 1, 0));
}

TArray<AActor*> UBeyondGameplayAbility::FindHostilesInRadius(FVector Center, float Radius) const
{
	TArray<AActor*> Result;
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = GetWorld();
	if (!Avatar || !World || Radius <= 0.0f)
	{
		return Result;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondHostilesInRadius), false, Avatar);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && !Result.Contains(Actor) && UBeyondCombatLibrary::AreHostile(Avatar, Actor) && !UBeyondCombatLibrary::IsActorDead(Actor))
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

USkeletalMeshComponent* UBeyondGameplayAbility::GetAnimatedMesh() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (const ABeyondCharacterBase* BeyondCharacter = Cast<ABeyondCharacterBase>(Avatar))
	{
		return BeyondCharacter->GetCombatMesh();
	}
	const ACharacter* Character = Cast<ACharacter>(Avatar);
	return Character ? Character->GetMesh() : nullptr;
}

float UBeyondGameplayAbility::PlayMontageOnAvatar(UAnimMontage* Montage, float PlayRate)
{
	const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	return (Montage && AnimInstance) ? AnimInstance->Montage_Play(Montage, PlayRate) : 0.0f;
}

const FGameplayTagContainer* UBeyondGameplayAbility::GetCooldownTags() const
{
	if (CooldownDuration > 0.0f && !CooldownGameplayEffectClass && !CooldownTags.IsEmpty())
	{
		return &CooldownTags;
	}
	return Super::GetCooldownTags();
}

void UBeyondGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownGameplayEffectClass || CooldownDuration <= 0.0f || CooldownTags.IsEmpty())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UBeyondGE_Cooldown::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
		Spec.Data->SetSetByCallerMagnitude(BeyondTags::SetByCaller_Duration, CooldownDuration * GetLevelCooldownScale(GetAbilityLevel(Handle, ActorInfo)));
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}

bool UBeyondGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC->HasMatchingGameplayTag(BeyondTags::State_Dead) || (!bUsableDuringDuo && ASC->HasMatchingGameplayTag(BeyondTags::State_Duo)))
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UBeyondGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (const ABeyondCharacterBase* Character = Cast<ABeyondCharacterBase>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (const TObjectPtr<UAnimMontage>* Montage = Character->AbilityMontages.Find(GetClass()); Montage && *Montage)
		{
			PlayMontageOnAvatar(*Montage);
		}
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UBeyondGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	++ActivationSerial;
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
}

void UBeyondGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (bActivateOnGranted && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}
