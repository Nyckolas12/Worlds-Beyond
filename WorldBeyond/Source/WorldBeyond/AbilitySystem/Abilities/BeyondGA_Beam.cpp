// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Beam.h"
#include "AbilitySystem/BeyondAreaStrike.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

UBeyondGA_Beam::UBeyondGA_Beam()
{
	SetAssetTags(FGameplayTagContainer(BeyondTags::Ability_Enemy_Attack));
	AIMinRange = 300.0f;
	AIMaxRange = 1200.0f;
	DamageType = BeyondTags::DamageType_Explosion;
}

void UBeyondGA_Beam::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BeamTarget = GetAIFocusTarget();
	bFiring = false;
	FiredFor = 0.0f;
	const FVector Toward = BeamTarget.IsValid() ? (BeamTarget->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D()
		: Character->GetActorForwardVector().GetSafeNormal2D();
	Character->SetActorRotation(FRotator(0.0f, Toward.Rotation().Yaw, 0.0f));
	Character->GetCharacterMovement()->StopMovementImmediately();
	if (AAIController* AI = Cast<AAIController>(Character->GetController()))
	{
		// The beam does its own (slow) turning
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}

	// The lane: fills during the wind-up, then stays lit while the beam fires (the ability deals the damage)
	FBeyondStrikeSettings LaneSettings;
	LaneSettings.Shape = EBeyondStrikeShape::Line;
	LaneSettings.Length = Length;
	LaneSettings.Width = Width;
	LaneSettings.WindUp = WindUp;
	LaneSettings.Damage = 0.0f;
	LaneSettings.LingerDuration = Duration;
	LaneSettings.Color = LaneColor;
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Lane = ABeyondAreaStrike::SpawnStrike(Character, Character, LaneSettings, Feet, Toward.Rotation().Yaw);

	PlayMontageOnAvatar(Montage, MontagePlayRate);
	GetWorld()->GetTimerManager().SetTimer(FireTimer, FTimerDelegate::CreateUObject(this, &ThisClass::StartFiring), FMath::Max(WindUp, 0.01f), false);
}

void UBeyondGA_Beam::StartFiring()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !IsActive())
	{
		return;
	}
	bFiring = true;
	BeamEffect = BeyondFX::SpawnAttached(BeamFX, Character->GetRootComponent());
	GetWorld()->GetTimerManager().SetTimer(TickTimer, FTimerDelegate::CreateUObject(this, &ThisClass::TickBeam), BeamTickInterval, true);
}

void UBeyondGA_Beam::TickBeam()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !IsActive() || UBeyondCombatLibrary::IsActorDead(Character))
	{
		return;
	}

	FiredFor += BeamTickInterval;
	if (FiredFor >= Duration)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Turn after the target, capped
	float Yaw = Character->GetActorRotation().Yaw;
	if (const AActor* Target = BeamTarget.Get(); Target && !UBeyondCombatLibrary::IsActorDead(Target))
	{
		const float Wanted = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;
		const float Step = TurnRate * BeamTickInterval;
		Yaw += FMath::Clamp(FRotator::NormalizeAxis(Wanted - Yaw), -Step, Step);
		Character->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	}

	ABeyondAreaStrike* LaneActor = Lane.Get();
	if (!LaneActor)
	{
		return;
	}
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	LaneActor->SetActorLocationAndRotation(Feet, FRotator(0.0f, Yaw, 0.0f));

	float Dealt = 0.0f;
	const float Damage = DamagePerSecond * BeamTickInterval * GetLevelDamageScale();
	for (AActor* Hostile : FindHostilesInRadius(Feet + Character->GetActorForwardVector() * Length * 0.5f, Length * 0.5f + Width))
	{
		if (LaneActor->IsInside(Hostile))
		{
			const float Before = UBeyondCombatLibrary::GetActorHealth(Hostile);
			UBeyondCombatLibrary::ApplyDamage(Character, Hostile, Damage, DamageType, HitResponse, true, Character, true);
			Dealt += FMath::Max(Before - UBeyondCombatLibrary::GetActorHealth(Hostile), 0.0f);
			if (FiredFor - LastHitFX >= 0.4f)
			{
				BeyondFX::SpawnAtLocation(this, HitFX, Hostile->GetActorLocation());
			}
		}
	}
	if (FiredFor - LastHitFX >= 0.4f)
	{
		LastHitFX = FiredFor;
	}
	if (DrainFraction > 0.0f && Dealt > 0.0f)
	{
		UBeyondCombatLibrary::ApplyHeal(Character, Character, Dealt * DrainFraction);
	}
}

void UBeyondGA_Beam::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bFiring = false;
	if (UFXSystemComponent* Effect = BeamEffect.Get())
	{
		Effect->DestroyComponent();
	}
	if (ABeyondAreaStrike* LaneActor = Lane.Get())
	{
		LaneActor->Destroy();
	}
	if (const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()); AI && BeamTarget.IsValid())
		{
			AI->SetFocus(BeamTarget.Get(), EAIFocusPriority::Gameplay);
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
