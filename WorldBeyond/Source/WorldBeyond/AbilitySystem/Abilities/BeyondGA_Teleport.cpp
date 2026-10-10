// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Teleport.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/BeyondEnemySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UBeyondGA_Teleport::UBeyondGA_Teleport()
{
	AIMinRange = 0.0f;
	AIMaxRange = 350.0f;
	AIWeight = 2.0f;
}

void UBeyondGA_Teleport::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const AActor* Target = GetAIFocusTarget();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector From = Character->GetActorLocation();
	FVector Destination = From - Character->GetActorForwardVector() * Distance;
	if (Target)
	{
		const FVector Away = (From - Target->GetActorLocation()).GetSafeNormal2D();
		Destination = Mode == EBeyondTeleportMode::AwayFromTarget
			? From + (Away.IsNearlyZero() ? -Character->GetActorForwardVector() : Away) * Distance
			: Target->GetActorLocation() - Target->GetActorForwardVector() * 220.0f;
	}

	FVector Ground;
	if (UBeyondEnemySubsystem::FindGroundPoint(GetWorld(), Destination, Ground))
	{
		Destination = Ground + FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0f);
	}

	BeyondFX::SpawnAtLocation(this, DepartFX, From);
	if (AAIController* AI = Cast<AAIController>(Character->GetController()))
	{
		AI->StopMovement();
	}
	const FRotator Facing = Target ? FRotator(0.0f, (Target->GetActorLocation() - Destination).GetSafeNormal2D().Rotation().Yaw, 0.0f) : Character->GetActorRotation();
	Character->TeleportTo(Destination, Facing);
	BeyondFX::SpawnAtLocation(this, ArriveFX, Character->GetActorLocation());

	const float Length = PlayMontageOnAvatar(Montage);
	GetWorld()->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}), FMath::Max(FMath::Min(Length, 0.6f), 0.3f), false);
}
