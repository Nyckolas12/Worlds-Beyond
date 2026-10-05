// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Brand.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	const FGameplayTag& ShootProjectileTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.ShootProjectile"));
		return Tag;
	}
}

UBeyondGA_Brand::UBeyondGA_Brand()
{
	InputTag = BeyondTags::Ability_Input_E;
	Brand.DetonateHitResponse = BeyondTags::Event_Hit_Stagger;
	AIMinRange = 300.0f;
	AIMaxRange = 1500.0f;
	AIWeight = 2.0f;
}

bool UBeyondGA_Brand::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// The AI shouldn't waste it re-branding a target that still carries a brand
	const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AAIController* AIController = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	if (AIController)
	{
		const UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(Pawn);
		const AActor* Target = AIController->GetFocusActor();
		return !(Combat && Target && Combat->IsBranded(Target));
	}
	return true;
}

AActor* UBeyondGA_Brand::FindTarget() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	if (AActor* FocusTarget = GetAIFocusTarget())
	{
		return UBeyondCombatLibrary::AreHostile(Avatar, FocusTarget) ? FocusTarget : nullptr;
	}

	const FVector Origin = Avatar->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector Aim = GetAimRotation(Origin).Vector();

	// Straight down the crosshair first
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondBrandAim), false, Avatar);
	GetWorld()->SweepMultiByObjectType(Hits, Origin, Origin + Aim * Range, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(60.0f), Params);
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && UBeyondCombatLibrary::AreHostile(Avatar, HitActor) && !UBeyondCombatLibrary::IsActorDead(HitActor))
		{
			return HitActor;
		}
	}

	// Otherwise the hostile closest to the crosshair direction
	AActor* Best = nullptr;
	float BestDot = FMath::Cos(FMath::DegreesToRadians(AimAssistAngle));
	for (AActor* Candidate : FindHostilesInRadius(Avatar->GetActorLocation(), Range))
	{
		const float Dot = FVector::DotProduct((Candidate->GetActorLocation() - Origin).GetSafeNormal(), Aim);
		if (Dot >= BestDot)
		{
			BestDot = Dot;
			Best = Candidate;
		}
	}
	return Best;
}

void UBeyondGA_Brand::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bApplied = false;
	bMontageDone = false;
	BrandTarget = FindTarget();

	// Nothing to brand: fizzle without spending the cooldown
	if (!BrandTarget.IsValid() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		const FVector ToTarget = (BrandTarget->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			Avatar->SetActorRotation(ToTarget.Rotation());
		}
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	ASC->GenericGameplayEventCallbacks.FindOrAdd(BeyondTags::Event_Montage_Trigger).AddUObject(this, &ThisClass::HandleCastEvent);
	ASC->GenericGameplayEventCallbacks.FindOrAdd(ShootProjectileTag()).AddUObject(this, &ThisClass::HandleCastEvent);

	const float Duration = PlayMontageOnAvatar(CastMontage);
	if (Duration > 0.0f)
	{
		if (UAnimInstance* AnimInstance = GetAnimatedMesh()->GetAnimInstance())
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &ThisClass::HandleMontageEnded, ActivationSerial);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, CastMontage);
		}
		GetWorld()->GetTimerManager().SetTimer(ApplyTimer, this, &ThisClass::ApplyBrand, FMath::Clamp(FallbackApplyDelay, 0.01f, Duration * 0.9f), false);
	}
	else
	{
		bMontageDone = true;
		ApplyBrand();
	}
}

void UBeyondGA_Brand::HandleCastEvent(const FGameplayEventData* Payload)
{
	ApplyBrand();
}

void UBeyondGA_Brand::ApplyBrand()
{
	if (bApplied || !IsActive())
	{
		return;
	}
	bApplied = true;
	GetWorld()->GetTimerManager().ClearTimer(ApplyTimer);
	UnbindCastEvents();

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (USkeletalMeshComponent* Mesh = GetAnimatedMesh())
	{
		BeyondFX::SpawnAttached(CastFX, Mesh, Mesh->DoesSocketExist(CastSocket) ? CastSocket : NAME_None);
	}

	AActor* Target = BrandTarget.Get();
	UBeyondCombatSubsystem* Combat = UBeyondCombatSubsystem::Get(Avatar);
	if (Target && Combat && !UBeyondCombatLibrary::IsActorDead(Target))
	{
		BeyondFX::SpawnAtLocation(this, ApplyFX, Target->GetActorLocation());
		Combat->ApplyBrand(Avatar, Target, Brand);
	}

	if (bMontageDone)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBeyondGA_Brand::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 Serial)
{
	if (!IsCurrentActivation(Serial))
	{
		return;
	}
	bMontageDone = true;
	if (!bApplied && !bInterrupted)
	{
		ApplyBrand();
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted && !bApplied);
}

void UBeyondGA_Brand::UnbindCastEvents()
{
	if (UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		for (const FGameplayTag& Tag : { FGameplayTag(BeyondTags::Event_Montage_Trigger), ShootProjectileTag() })
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Tag))
			{
				Delegate->RemoveAll(this);
			}
		}
	}
}

void UBeyondGA_Brand::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyTimer);
	}
	UnbindCastEvents();
	BrandTarget.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
