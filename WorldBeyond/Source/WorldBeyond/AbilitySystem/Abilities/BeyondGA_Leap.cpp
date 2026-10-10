// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Leap.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "BeyondGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UBeyondGA_Leap::UBeyondGA_Leap()
{
	SetAssetTags(FGameplayTagContainer(BeyondTags::Ability_Enemy_Attack));
	AIMinRange = 400.0f;
	AIMaxRange = 1300.0f;
	Landing.Shape = EBeyondStrikeShape::Circle;
	Landing.Radius = 340.0f;
	Landing.Damage = 45.0f;
	Landing.DamageType = BeyondTags::DamageType_Melee;
	Landing.HitResponse = BeyondTags::Event_Hit_KnockBack;
}

void UBeyondGA_Leap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const AActor* Target = GetAIFocusTarget();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Lock the landing spot just short of the target
	const FVector Start = Character->GetActorLocation();
	FVector Goal = Target ? Target->GetActorLocation() : Start + Character->GetActorForwardVector() * AIMaxRange * 0.6f;
	const FVector Toward = (Goal - Start).GetSafeNormal2D();
	Goal -= Toward * LandShortBy;
	FVector Ground = Goal;
	UBeyondEnemySubsystem::FindGroundPoint(GetWorld(), Goal, Ground);
	LandingPoint = Ground;
	Character->SetActorRotation(FRotator(0.0f, Toward.Rotation().Yaw, 0.0f));

	if (AAIController* AI = Cast<AAIController>(Character->GetController()))
	{
		AI->StopMovement();
	}

	FBeyondStrikeSettings Settings = Landing;
	Settings.WindUp = TakeOffDelay + AirTime;
	ABeyondAreaStrike::SpawnStrike(Character, Character, Settings, LandingPoint, Toward.Rotation().Yaw, 0.0f, GetLevelDamageScale());

	// The jump animation's own root motion would fight the launch (and drift during the wind-up)
	if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
	{
		SavedRootMotionMode = Anim->RootMotionMode;
		Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		bChangedRootMotion = true;
	}

	PlayMontageOnAvatar(Montage, MontagePlayRate);
	if (TakeOffDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(TakeOffTimer, FTimerDelegate::CreateUObject(this, &ThisClass::TakeOff), TakeOffDelay, false);
	}
	else
	{
		TakeOff();
	}
}

void UBeyondGA_Leap::TakeOff()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !IsActive())
	{
		return;
	}

	// Ballistic arc: horizontal speed covers the distance in Air Time, vertical speed fights gravity for it
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Gravity = FMath::Abs(Movement->GetGravityZ());
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Delta = LandingPoint - Feet;
	FVector Velocity = FVector(Delta.X, Delta.Y, 0.0f) / AirTime;
	Velocity.Z = (Delta.Z + 0.5f * Gravity * AirTime * AirTime) / AirTime;
	Character->LaunchCharacter(Velocity, true, true);

	GetWorld()->GetTimerManager().SetTimer(LandTimer, FTimerDelegate::CreateUObject(this, &ThisClass::Land), AirTime + 0.1f, false);
}

void UBeyondGA_Leap::Land()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBeyondGA_Leap::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bChangedRootMotion)
	{
		const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->SetRootMotionMode(SavedRootMotionMode);
		}
		bChangedRootMotion = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
