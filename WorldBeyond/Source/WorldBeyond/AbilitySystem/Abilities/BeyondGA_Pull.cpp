// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_Pull.h"
#include "AbilitySystem/BeyondCombatLibrary.h"
#include "AIController.h"
#include "BeyondGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UBeyondGA_Pull::UBeyondGA_Pull()
{
	SetAssetTags(FGameplayTagContainer(BeyondTags::Ability_Enemy_Attack));
	AIMinRange = 450.0f;
	AIMaxRange = 1250.0f;
	Line.Shape = EBeyondStrikeShape::Line;
	Line.Length = 1300.0f;
	Line.Width = 150.0f;
	Line.WindUp = 0.9f;
	Line.Damage = 15.0f;
	Line.DamageType = BeyondTags::DamageType_Explosion;
	Line.HitResponse = BeyondTags::Event_Hit_Light;
	Line.Color = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f);
}

void UBeyondGA_Pull::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AActor* Target = GetAIFocusTarget();
	const FVector Toward = Target ? (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D()
		: Character->GetActorForwardVector().GetSafeNormal2D();
	Character->SetActorRotation(FRotator(0.0f, Toward.Rotation().Yaw, 0.0f));
	if (AAIController* AI = Cast<AAIController>(Character->GetController()))
	{
		AI->StopMovement();
	}

	FBeyondStrikeSettings Settings = Line;
	Settings.Shape = EBeyondStrikeShape::Line;
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Tether = ABeyondAreaStrike::SpawnStrike(Character, Character, Settings, Feet, Toward.Rotation().Yaw, 0.0f, GetLevelDamageScale());
	LastPulled = 0;

	PlayMontageOnAvatar(Montage);
	// Just before the line lands: whoever stands in it now gets yanked
	GetWorld()->GetTimerManager().SetTimer(YankTimer, FTimerDelegate::CreateUObject(this, &ThisClass::Yank), FMath::Max(Settings.WindUp - 0.03f, 0.01f), false);
	GetWorld()->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}), Settings.WindUp + PullDuration + 0.2f, false);
}

void UBeyondGA_Pull::Yank()
{
	const ACharacter* Caster = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const ABeyondAreaStrike* Strike = Tether.Get();
	if (!Caster || !Strike || !IsActive())
	{
		return;
	}

	for (AActor* Hostile : FindHostilesInRadius(Strike->GetActorLocation() + Strike->GetActorForwardVector() * Line.Length * 0.5f, Line.Length * 0.5f + Line.Width))
	{
		ACharacter* Victim = Cast<ACharacter>(Hostile);
		if (!Victim || !Strike->IsInside(Victim))
		{
			continue;
		}
		const FVector ToCaster = Caster->GetActorLocation() - Victim->GetActorLocation();
		const float Travel = FMath::Max(ToCaster.Size2D() - StopDistance, 0.0f);
		const FVector Velocity = ToCaster.GetSafeNormal2D() * (Travel / PullDuration) + FVector(0.0f, 0.0f, 250.0f);
		Victim->LaunchCharacter(Velocity, true, true);
		BeyondFX::SpawnAttached(PullFX, Victim->GetRootComponent());
		++LastPulled;
	}
}
