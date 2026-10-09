// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/BeyondGA_AreaAttack.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BeyondGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UBeyondGA_AreaAttack::UBeyondGA_AreaAttack()
{
	SetAssetTags(FGameplayTagContainer(BeyondTags::Ability_Enemy_Attack));
	AIMinRange = 0.0f;
	AIMaxRange = 900.0f;
	Strike.DamageType = BeyondTags::DamageType_Explosion;
	Strike.HitResponse = BeyondTags::Event_Hit_Stagger;
}

TArray<ABeyondAreaStrike*> UBeyondGA_AreaAttack::GetLastStrikes() const
{
	TArray<ABeyondAreaStrike*> Result;
	for (const TWeakObjectPtr<ABeyondAreaStrike>& Spawned : LastStrikes)
	{
		if (ABeyondAreaStrike* Alive = Spawned.Get())
		{
			Result.Add(Alive);
		}
	}
	return Result;
}

FVector UBeyondGA_AreaAttack::ProjectToGround(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Location;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeyondAreaAttackGround), false, GetAvatarActorFromActorInfo());
	const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllStaticObjects);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, 250.0f), Location - FVector(0.0f, 0.0f, 1200.0f), Objects, Params))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void UBeyondGA_AreaAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	ACharacter* Character = Cast<ACharacter>(Avatar);
	if (!Avatar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Lock where it lands now: the telegraph is the promise, moving out of it is the counterplay
	const AActor* Target = GetAIFocusTarget();
	const FVector SelfLocation = Avatar->GetActorLocation();
	const FVector TargetLocation = Target ? Target->GetActorLocation() : SelfLocation + Avatar->GetActorForwardVector() * FMath::Min(AIMaxRange, 600.0f);
	FVector Facing = (TargetLocation - SelfLocation).GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		Facing = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	const float Yaw = Facing.Rotation().Yaw;
	Avatar->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));

	// Plant the feet for the swing
	if (Character)
	{
		if (AAIController* AI = Cast<AAIController>(Character->GetController()))
		{
			AI->StopMovement();
		}
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	FVector AimPoint = SelfLocation;
	switch (Aim)
	{
	case EBeyondStrikeAim::AtTarget:
		AimPoint = TargetLocation;
		break;
	case EBeyondStrikeAim::AtSelf:
	case EBeyondStrikeAim::FromSelfTowardTarget:
		AimPoint = SelfLocation;
		break;
	}

	BeyondFX::SpawnAttached(CastFX, Avatar->GetRootComponent());

	float Duration = 0.0f;
	if (Montage)
	{
		Duration = PlayMontageOnAvatar(Montage, MontagePlayRate) / FMath::Max(MontagePlayRate, 0.1f);
		if (Duration > 0.0f)
		{
			PlayingMontage = Montage;
		}
	}

	LastStrikes.Reset();
	const float DamageScale = GetLevelDamageScale();
	for (int32 Index = 0; Index < FMath::Max(Count, 1); ++Index)
	{
		FVector Point = AimPoint;
		if (Index > 0 && Scatter > 0.0f)
		{
			const FVector2D Offset = FMath::RandPointInCircle(Scatter);
			Point += FVector(Offset.X, Offset.Y, 0.0f);
		}

		FBeyondStrikeSettings Settings = Strike;
		Settings.Radius += RadiusGrowth * Index;
		if (Settings.Shape == EBeyondStrikeShape::Ring && RadiusGrowth > 0.0f)
		{
			Settings.InnerRadius += RadiusGrowth * Index;
		}

		const float Delay = FirstStrikeDelay + Interval * Index;
		if (ABeyondAreaStrike* Spawned = ABeyondAreaStrike::SpawnStrike(Avatar, Avatar, Settings, ProjectToGround(Point), Yaw, Delay, DamageScale))
		{
			LastStrikes.Add(Spawned);
		}
	}

	// Committed until the swing ends (or the last marker is well under way)
	const float StrikesTime = FirstStrikeDelay + Interval * (FMath::Max(Count, 1) - 1) + Strike.WindUp * 0.6f;
	const float Total = FMath::Max3(Duration, StrikesTime, MinDuration);
	GetWorld()->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}), FMath::Max(Total, 0.05f), false);
}

void UBeyondGA_AreaAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndTimer);
	}

	// Cancelled (hit reaction, death): stop the swing; the markers already down still land
	if (bWasCancelled)
	{
		if (UAnimMontage* Playing = PlayingMontage.Get())
		{
			const USkeletalMeshComponent* Mesh = GetAnimatedMesh();
			if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr; AnimInstance && AnimInstance->Montage_IsPlaying(Playing))
			{
				AnimInstance->Montage_Stop(0.2f, Playing);
			}
		}
	}
	PlayingMontage.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
