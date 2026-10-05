// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Dash.h"
#include "WorldBeyond.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/RootMotionSource.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

UBeyondGA_Dash::UBeyondGA_Dash()
{
	InputTag = BeyondTags::Ability_Input_Q;
	PathDamageType = BeyondTags::DamageType_Melee;
	PathHitResponse = BeyondTags::Event_Hit_Stagger;
	// A gap closer for the AI: dash in when the target is a little too far for the sword
	AIMinRange = 400.0f;
	AIMaxRange = 900.0f;
	AIWeight = 1.0f;
}

FVector UBeyondGA_Dash::ChooseDirection() const
{
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!Pawn)
	{
		return FVector::ForwardVector;
	}

	if (const AActor* Target = GetAIFocusTarget())
	{
		const FVector ToTarget = (Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget;
		}
	}

	// Players dash where they are steering; standing still dashes forward
	const FVector Input = Pawn->GetLastMovementInputVector().GetSafeNormal2D();
	return Input.IsNearlyZero() ? Pawn->GetActorForwardVector().GetSafeNormal2D() : Input;
}

void UBeyondGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogBeyond, Verbose, TEXT("%s: dash not committed (cost / cooldown)"), *GetNameSafe(ActorInfo->AvatarActor.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Crossed.Reset();
	bDashing = true;
	DashStart = Character->GetActorLocation();

	const FVector Direction = ChooseDirection();
	Character->SetActorRotation(Direction.Rotation());

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	ASC->AddLooseGameplayTag(BeyondTags::State_Dashing);
	bAddedInvincible = bInvincibleWhileDashing;
	if (bAddedInvincible)
	{
		ASC->AddLooseGameplayTag(BeyondTags::State_Invincible);
	}

	if (bPassThroughPawns)
	{
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		PreviousPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

		// Character meshes and held weapons can use other collision channels; ignore those actors outright
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondDashIgnore), false, Character);
		GetWorld()->OverlapMultiByObjectType(Overlaps, Character->GetActorLocation() + Direction * DashDistance * 0.5f, FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), FCollisionShape::MakeSphere(DashDistance * 0.75f), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Other = Overlap.GetActor();
			APawn* OtherPawn = Cast<APawn>(Other);
			if (!OtherPawn && Other && Other->GetAttachParentActor())
			{
				OtherPawn = Cast<APawn>(Other->GetAttachParentActor());
			}
			if (Other && OtherPawn && !IgnoredActors.Contains(Other))
			{
				Capsule->IgnoreActorWhenMoving(Other, true);
				IgnoredActors.Add(Other);
			}
		}
	}

	Trail = BeyondFX::SpawnAttached(TrailFX, GetAnimatedMesh() ? static_cast<USceneComponent*>(GetAnimatedMesh()) : Character->GetRootComponent(), TrailSocket);

	// Montage root motion would override the dash force (character movement lets anim root motion win)
	if (DashMontage)
	{
		if (UAnimInstance* AnimInstance = GetAnimatedMesh() ? GetAnimatedMesh()->GetAnimInstance() : nullptr)
		{
			PreviousRootMotionMode = AnimInstance->RootMotionMode;
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			bChangedRootMotionMode = true;
		}
		PlayMontageOnAvatar(DashMontage, MontagePlayRate);
	}

	UAbilityTask_ApplyRootMotionConstantForce* Force = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, NAME_None, Direction, DashDistance / DashDuration, DashDuration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, ExitSpeed, false);
	Force->OnFinish.AddDynamic(this, &ThisClass::HandleDashFinished);
	Force->ReadyForActivation();
	UE_LOG(LogBeyond, Verbose, TEXT("%s: dash %s, %.0f uu over %.2f s"), *Character->GetName(), *Direction.ToCompactString(), DashDistance, DashDuration);

	// The task may never report back if something else overrides movement
	GetWorld()->GetTimerManager().SetTimer(SafetyTimer, this, &ThisClass::HandleDashFinished, DashDuration + 0.25f, false);
}

void UBeyondGA_Dash::HandleDashFinished()
{
	if (!bDashing || !IsActive())
	{
		return;
	}
	bDashing = false;
	GetWorld()->GetTimerManager().ClearTimer(SafetyTimer);
	UE_LOG(LogBeyond, Verbose, TEXT("%s: dash finished after %.0f uu"), *GetNameSafe(GetAvatarActorFromActorInfo()),
		GetAvatarActorFromActorInfo() ? FVector::Dist2D(DashStart, GetAvatarActorFromActorInfo()->GetActorLocation()) : 0.0f);

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar && PathDamage > 0.0f)
	{
		// Everyone standing along the dash line
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondDashPath), false, Avatar);
		GetWorld()->SweepMultiByObjectType(Hits, DashStart, Avatar->GetActorLocation(), FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(PathRadius), Params);
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && !Crossed.Contains(HitActor) && UBeyondCombatLibrary::AreHostile(Avatar, HitActor) && !UBeyondCombatLibrary::IsActorDead(HitActor))
			{
				Crossed.Add(HitActor);
			}
		}
	}

	RestoreCharacter();

	if (Crossed.IsEmpty())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(DetonateTimer, this, &ThisClass::Detonate, FMath::Max(DetonateDelay, 0.01f), false);
}

void UBeyondGA_Dash::Detonate()
{
	for (AActor* Target : Crossed)
	{
		if (Target && !UBeyondCombatLibrary::IsActorDead(Target))
		{
			BeyondFX::SpawnAtLocation(this, DetonateFX, Target->GetActorLocation());
			ApplyDamageToTarget(Target, PathDamage, PathDamageType, PathHitResponse);
		}
	}
	Crossed.Reset();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBeyondGA_Dash::RestoreCharacter()
{
	if (UFXSystemComponent* TrailComponent = Trail.Get())
	{
		TrailComponent->Deactivate();
	}
	Trail.Reset();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && ASC->HasMatchingGameplayTag(BeyondTags::State_Dashing))
	{
		ASC->RemoveLooseGameplayTag(BeyondTags::State_Dashing);
		if (bAddedInvincible)
		{
			ASC->RemoveLooseGameplayTag(BeyondTags::State_Invincible);
		}
	}
	bAddedInvincible = false;

	if (bChangedRootMotionMode)
	{
		bChangedRootMotionMode = false;
		if (UAnimInstance* AnimInstance = GetAnimatedMesh() ? GetAnimatedMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->SetRootMotionMode(PreviousRootMotionMode);
		}
	}

	if (bPassThroughPawns)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnResponse);
			for (const TWeakObjectPtr<AActor>& Other : IgnoredActors)
			{
				if (Other.IsValid())
				{
					Capsule->IgnoreActorWhenMoving(Other.Get(), false);
				}
			}
		}
		IgnoredActors.Reset();
	}
}

void UBeyondGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetonateTimer);
		World->GetTimerManager().ClearTimer(SafetyTimer);
	}
	if (bDashing)
	{
		bDashing = false;
		RestoreCharacter();
	}
	Crossed.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
